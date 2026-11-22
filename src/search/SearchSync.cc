#include "SearchSync.hpp"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include <unistd.h>

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Socket/Address.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "DB/ArticleCategoryRelInfo.hpp"
#include "DB/ArticleInfo.hpp"
#include "DB/ArticleLabelRelInfo.hpp"

#include "../Repository/ArticleQueryRepository.hpp"
#include "../Repository/ArticleRelRepository.hpp"
#include "../Job/RedisLease.hpp"
#include "../Config/AppConfig.hpp"
#include "../Config/SearchConfig.hpp"
#include "../Util/Util.hpp"
#include "../Repository/ArticleRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static bool ParseDirtyValue(const std::string& value, int32_t& op, int64_t& ts)
{
    auto pos = value.find(':');
    if (pos == std::string::npos) {
        op = static_cast<int32_t>(chen::TypeUtil::Atoi(value));
        ts = 0;
        return true;
    }
    op = static_cast<int32_t>(chen::TypeUtil::Atoi(value.substr(0, pos)));
    ts = chen::TypeUtil::Atoi(value.substr(pos + 1));
    return true;
}

bool SearchSync::init()
{
    const SearchConf conf = GetSearchConf();
    enabled_.store(conf.enabled);
    if (!enabled_.load()) {
        INFO(logger) << "search client disabled";
        return true;
    }

    stopped_.store(false);
    auto iom = chen::IOManager::GetThis();
    if (!iom) {
        ERROR(logger) << "search client init failed: no IOManager";
        enabled_.store(false);
        return false;
    }

    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }
    uint64_t interval = conf.flush_interval_ms;
    if (interval == 0) {
        interval = 1000;
    }
    timer_ = iom->addTimer(
        interval,
        [this]() {
            wake();
        },
        true);
    wake();
    INFO(logger) << "search client initialized, hub=" << conf.hub_address;
    return true;
}

void SearchSync::stop()
{
    stopped_.store(true);
    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }
    invalidateClient();
}

void SearchSync::invalidateClient()
{
    std::lock_guard<std::mutex> lock(client_mutex_);
    if (client_) {
        client_->close();
        client_.reset();
    }
    last_stats_ms_.store(0);
}

bool SearchSync::ensureConnected()
{
    if (!enabled_.load() || stopped_.load()) {
        return false;
    }

    const SearchConf conf = GetSearchConf();
    std::lock_guard<std::mutex> lock(client_mutex_);
    if (client_ && client_->isConnected()) {
        return true;
    }

    if (client_) {
        client_->close();
        client_.reset();
    }

    uint64_t now_ms = chen::TimeUtil::GetCurrentMs();
    uint64_t reconnect_interval = conf.reconnect_interval_ms;
    if (reconnect_interval > 0 && now_ms < last_connect_ms_.load() + reconnect_interval) {
        return false;
    }
    last_connect_ms_.store(now_ms);

    auto addr = chen::Address::LookupAny(conf.hub_address);
    if (!addr) {
        ERROR(logger) << "invalid search hub address: " << conf.hub_address;
        return false;
    }

    auto client = std::make_shared<chen::rpc::RpcClient>();
    if (!client->connect(addr, conf.connect_timeout_ms)) {
        DEBUG(logger) << "connect search hub failed: " << conf.hub_address;
        return false;
    }
    client_ = client;
    last_stats_ms_.store(0);
    INFO(logger) << "connected search hub: " << conf.hub_address;
    return true;
}

void SearchSync::wake()
{
    if (!enabled_.load() || stopped_.load()) {
        return;
    }
    if (sync_scheduled_.exchange(true)) {
        return;
    }

    auto iom = chen::IOManager::GetThis();
    if (!iom) {
        sync_scheduled_.store(false);
        return;
    }
    iom->schedule([this]() {
        sync_scheduled_.store(false);
        syncLoop();
    });
}

void SearchSync::syncLoop()
{
    if (!enabled_.load() || stopped_.load()) {
        return;
    }
    if (syncing_.exchange(true)) {
        return;
    }

    const SearchConf conf = GetSearchConf();

    try {
        do {
            uint64_t now_ms = chen::TimeUtil::GetCurrentMs();
            bool need_stats = now_ms >= last_stats_ms_.load() + conf.stats_interval_ms;
            if (need_stats) {
                last_stats_ms_.store(now_ms);
            }

            if (!ensureConnected()) {
                break;
            }
            // 新建连接后强制做一次 stats，避免 SearchServer 重启后长时间返回未就绪
            if (last_stats_ms_.load() == 0) {
                need_stats = true;
                last_stats_ms_.store(now_ms);
            }

            if (need_stats) {
                tagSearchStats stats;
                if (!checkStats(stats)) {
                    break;
                }
                if (!stats.Ready || stats.IndexVersion != SS_SEARCH_INDEX_VERSION) {
                    INFO(logger) << "search index not ready, ready=" << stats.Ready << " version=" << stats.IndexVersion << ", start snapshot";
                    if (!runSnapshot()) {
                        last_stats_ms_.store(0);
                        break;
                    }
                    last_stats_ms_.store(chen::TimeUtil::GetCurrentMs());
                }
            }

            flushDirty();
        } while (0);
    } catch (const std::exception& e) {
        ERROR(logger) << "search sync loop failed: " << e.what();
        last_stats_ms_.store(0);
        invalidateClient();
    } catch (...) {
        ERROR(logger) << "search sync loop failed: unknown error";
        last_stats_ms_.store(0);
        invalidateClient();
    }

    syncing_.store(false);
}

bool SearchSync::checkStats(tagSearchStats& stats)
{
    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client = client_;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        stats = client->callWithTimeout<tagSearchStats>(
            GetSearchConf().timeout_ms, static_cast<uint32_t>(SS_CMD_SEARCH_GET_STATS));
        return stats.Code == SEARCH_OK;
    } catch (const std::exception& e) {
        WARN(logger) << "search stats failed: " << e.what();
        invalidateClient();
        return false;
    }
}

bool SearchSync::query(const tagSearchQuery& query, tagSearchResult& result)
{
    if (!enabled_.load() || stopped_.load()) {
        return false;
    }
    if (!ensureConnected()) {
        return false;
    }

    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client = client_;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        result = client->callWithTimeout<tagSearchResult>(
            GetSearchConf().timeout_ms, static_cast<uint32_t>(SS_CMD_SEARCH_ARTICLES), query);
        if (result.Code != SEARCH_OK) {
            WARN(logger) << "search query rejected, code=" << result.Code;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "search query failed: " << e.what();
        invalidateClient();
        return false;
    }
}

void SearchSync::markDirty(uint64_t article_id, int32_t op)
{
    if (!enabled_.load() || stopped_.load() || article_id == 0) {
        return;
    }

    DirtyEntry entry;
    entry.op = op;
    entry.ts = static_cast<int64_t>(chen::TimeUtil::GetCurrentMs());

    {
        std::lock_guard<std::mutex> lock(dirty_mutex_);
        auto it = local_dirty_.find(article_id);
        if (it == local_dirty_.end() || entry.ts >= it->second.ts) {
            local_dirty_[article_id] = entry;
        }
    }
    cacheDirty(article_id, entry);
    wake();
}

void SearchSync::cacheDirty(uint64_t article_id, const DirtyEntry& entry)
{
    static const char* kCacheScript = "local cur = redis.call('hget', KEYS[1], ARGV[1]) "
                                      "if cur then "
                                      "  local i = string.find(cur, ':') "
                                      "  if i then "
                                      "    local t = tonumber(string.sub(cur, i + 1)) "
                                      "    if t and t > tonumber(ARGV[2]) then return 0 end "
                                      "  end "
                                      "end "
                                      "return redis.call('hset', KEYS[1], ARGV[1], ARGV[3])";
    const SearchConf conf = GetSearchConf();
    std::string value = std::to_string(entry.op) + ":" + std::to_string(entry.ts);
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "eval %s 1 %s %llu %lld %s", kCacheScript,
        conf.dirty_key.c_str(), (unsigned long long)article_id, (long long)entry.ts, value.c_str());
    if (!rpy) {
        WARN(logger) << "cache dirty article to redis failed, article_id=" << article_id;
    }
}

std::map<uint64_t, SearchSync::DirtyEntry> SearchSync::loadDirty()
{
    std::map<uint64_t, DirtyEntry> dirty;
    {
        std::lock_guard<std::mutex> lock(dirty_mutex_);
        dirty = local_dirty_;
    }

    const SearchConf conf = GetSearchConf();
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hgetall %s", conf.dirty_key.c_str());
    if (rpy && rpy->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < rpy->elements; i += 2) {
            if (!rpy->element[i] || !rpy->element[i + 1] || !rpy->element[i]->str || !rpy->element[i + 1]->str) {
                continue;
            }
            uint64_t article_id = static_cast<uint64_t>(chen::TypeUtil::Atoi(rpy->element[i]->str));
            DirtyEntry entry;
            ParseDirtyValue(rpy->element[i + 1]->str, entry.op, entry.ts);
            auto it = dirty.find(article_id);
            if (it == dirty.end() || entry.ts >= it->second.ts) {
                dirty[article_id] = entry;
            }
        }
    }
    return dirty;
}

void SearchSync::ackDirty(uint64_t article_id, const DirtyEntry& entry)
{
    {
        std::lock_guard<std::mutex> lock(dirty_mutex_);
        auto it = local_dirty_.find(article_id);
        if (it != local_dirty_.end() && it->second.ts <= entry.ts) {
            local_dirty_.erase(it);
        }
    }

    static const char* kAckScript = "if redis.call('hget', KEYS[1], ARGV[1]) == ARGV[2] then "
                                    "return redis.call('hdel', KEYS[1], ARGV[1]) end return 0";
    const SearchConf conf = GetSearchConf();
    chen::RedisUtil::Cmd(GetRedisPoolName(), "eval %s 1 %s %llu %d:%lld", kAckScript, conf.dirty_key.c_str(),
        (unsigned long long)article_id, entry.op, (long long)entry.ts);
}

bool SearchSync::buildDoc(uint64_t article_id, int64_t update_time, tagSearchDoc& doc, bool& missing)
{
    missing = false;
    auto db = GetDB();
    if (!db) {
        return false;
    }

    auto info = ArticleRepository::Get(static_cast<int64_t>(article_id), GetDB());
    if (!info || info->getIsDeleted()) {
        missing = true;
        return false;
    }

    doc = tagSearchDoc();
    doc.ArticleId = static_cast<uint64_t>(info->getId());
    doc.UserId = info->getUserId();
    doc.State = info->getState();
    doc.Channel = info->getChannel();
    doc.PublishTime = info->getPublishTime();
    doc.Weight = info->getWeight();
    doc.SearchUpdateTime = update_time;
    doc.Title = info->getTitle();
    doc.Content = info->getContent();

    std::vector<DB::ArticleCategoryRelInfo::ptr> categories;
    if (!ArticleCategoryRelRepository::ListByArticle(categories, info->getId(), true, GetDB())) {
        return false;
    }
    for (auto& category : categories) {
        doc.CategoryIds.push_back(static_cast<uint64_t>(category->getCategoryId()));
    }

    std::vector<DB::ArticleLabelRelInfo::ptr> labels;
    if (!ArticleLabelRelRepository::ListByArticle(labels, info->getId(), true, GetDB())) {
        return false;
    }
    for (auto& label : labels) {
        doc.LabelIds.push_back(static_cast<uint64_t>(label->getLabelId()));
    }
    return true;
}

bool SearchSync::sendUpsert(const tagSearchDoc& doc)
{
    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client = client_;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        int32_t rt = client->callWithTimeout<int32_t>(
            GetSearchConf().update_timeout_ms, static_cast<uint32_t>(SS_CMD_SEARCH_UPSERT_DOC), doc);
        if (rt != SEARCH_OK) {
            WARN(logger) << "upsert search doc rejected, article_id=" << doc.ArticleId << ", code=" << rt;
            if (rt == SEARCH_NOT_READY) {
                last_stats_ms_.store(0);
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "upsert search doc failed, article_id=" << doc.ArticleId << ": " << e.what();
        invalidateClient();
        return false;
    }
}

bool SearchSync::sendRemove(uint64_t article_id, int64_t update_time)
{
    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client = client_;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        int32_t rt = client->callWithTimeout<int32_t>(GetSearchConf().update_timeout_ms,
            static_cast<uint32_t>(SS_CMD_SEARCH_REMOVE_ARTICLE), article_id, update_time);
        if (rt != SEARCH_OK) {
            WARN(logger) << "remove search article rejected, article_id=" << article_id << ", code=" << rt;
            if (rt == SEARCH_NOT_READY) {
                last_stats_ms_.store(0);
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "remove search article failed, article_id=" << article_id << ": " << e.what();
        invalidateClient();
        return false;
    }
}

bool SearchSync::flushDirty()
{
    auto dirty = loadDirty();
    if (dirty.empty()) {
        return true;
    }

    if (!GetDB()) {
        WARN(logger) << "flush search dirty failed: no db connection";
        return false;
    }

    std::vector<std::pair<int64_t, uint64_t>> order;
    order.reserve(dirty.size());
    for (auto& [article_id, entry] : dirty) {
        order.emplace_back(entry.ts, article_id);
    }
    std::sort(order.begin(), order.end());

    int32_t limit = GetSearchConf().flush_batch_size;
    if (limit <= 0) {
        limit = 200;
    }

    int32_t handled = 0;
    for (auto& item : order) {
        uint64_t article_id = item.second;
        if (handled >= limit) {
            break;
        }
        if (stopped_.load()) {
            break;
        }

        auto& entry = dirty[article_id];
        bool ok = false;
        if (entry.op == OP_REMOVE) {
            ok = sendRemove(article_id, entry.ts);
        } else {
            tagSearchDoc doc;
            bool missing = false;
            if (buildDoc(article_id, entry.ts, doc, missing)) {
                ok = sendUpsert(doc);
            } else if (missing) {
                ok = sendRemove(article_id, entry.ts);
            } else {
                WARN(logger) << "build search doc failed, retry later, article_id=" << article_id;
                return false;
            }
        }

        if (!ok) {
            return false;
        }
        ackDirty(article_id, entry);
        ++handled;
    }
    return true;
}

bool SearchSync::runSnapshot()
{
    if (!ensureConnected()) {
        return false;
    }

    chen::rpc::RpcClient::ptr client;
    {
        std::lock_guard<std::mutex> lock(client_mutex_);
        client = client_;
    }
    if (!client || !client->isConnected()) {
        return false;
    }

    // 全量重建只允许一个 worker 执行，避免多 worker 同时推快照
    RedisLease lease("search:snapshot", 1800);
    if (!lease.TryAcquire()) {
        INFO(logger) << "search snapshot is running on another worker, skip";
        return true;
    }

    uint64_t snapshot_id = (chen::TimeUtil::GetCurrentMs() << 8) ^ (static_cast<uint64_t>(::getpid()) & 0xff);
    const SearchConf conf = GetSearchConf();
    uint64_t rpc_timeout = conf.snapshot_rpc_timeout_ms;

    try {
        int64_t started_at = static_cast<int64_t>(chen::TimeUtil::GetCurrentMs());
        int32_t rt = client->callWithTimeout<int32_t>(rpc_timeout, static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_BEGIN), snapshot_id, started_at);
        if (rt != SEARCH_OK) {
            WARN(logger) << "search rebuild begin rejected, code=" << rt;
            return false;
        }

        int64_t last_id = 0;
        std::vector<tagSearchDoc> batch;
        size_t batch_bytes = 0;
        uint64_t total_docs = 0;

        auto flush_batch = [&]() -> bool {
            if (batch.empty()) {
                return true;
            }
            int32_t code = client->callWithTimeout<int32_t>(rpc_timeout, static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_DOCS), snapshot_id, batch);
            if (code != SEARCH_OK) {
                WARN(logger) << "search rebuild docs rejected, snapshot_id=" << snapshot_id << ", code=" << code;
                return false;
            }
            total_docs += batch.size();
            batch.clear();
            batch_bytes = 0;
            return true;
        };

        int32_t page_size = conf.snapshot_page_size;
        if (page_size <= 0) {
            page_size = 200;
        }
        size_t batch_limit = conf.snapshot_batch_bytes;
        if (batch_limit == 0) {
            batch_limit = 524288;
        }

        while (true) {
            if (stopped_.load()) {
                WARN(logger) << "search snapshot aborted: client stopped";
                return false;
            }
            auto db = GetDB();
            if (!db) {
                ERROR(logger) << "search snapshot failed: no db connection";
                return false;
            }

            std::vector<DB::ArticleInfo::ptr> infos;
            if (!ArticleQueryRepository::ListSnapshotAfter(last_id, page_size, infos, db)) {
                ERROR(logger) << "search snapshot query article failed, last_id=" << last_id;
                return false;
            }
            if (infos.empty()) {
                break;
            }

            std::vector<int64_t> ids;
            ids.reserve(infos.size());
            for (auto& info : infos) {
                ids.push_back(info->getId());
            }

            std::map<int64_t, std::vector<uint64_t>> category_map;
            std::vector<DB::ArticleCategoryRelInfo::ptr> categories;
            if (!ArticleCategoryRelRepository::ListByArticles(categories, ids, db)) {
                ERROR(logger) << "search snapshot query category rel failed";
                return false;
            }
            for (auto& rel : categories) {
                category_map[rel->getArticleId()].push_back(static_cast<uint64_t>(rel->getCategoryId()));
            }

            std::map<int64_t, std::vector<uint64_t>> label_map;
            std::vector<DB::ArticleLabelRelInfo::ptr> labels;
            if (!ArticleLabelRelRepository::ListByArticles(labels, ids, db)) {
                ERROR(logger) << "search snapshot query label rel failed";
                return false;
            }
            for (auto& rel : labels) {
                label_map[rel->getArticleId()].push_back(static_cast<uint64_t>(rel->getLabelId()));
            }

            for (auto& info : infos) {
                tagSearchDoc doc;
                doc.ArticleId = static_cast<uint64_t>(info->getId());
                doc.UserId = info->getUserId();
                doc.State = info->getState();
                doc.Channel = info->getChannel();
                doc.PublishTime = info->getPublishTime();
                doc.Weight = info->getWeight();
                // 快照文档统一使用快照开始时间：
                // - 快照开始后的增量更新（ts > started_at）会在 commit 时覆盖快照
                // - 快照开始前的旧脏数据（ts <= started_at）由其后的快照内容覆盖，应被忽略
                doc.SearchUpdateTime = started_at;
                doc.Title = info->getTitle();
                doc.Content = info->getContent();

                auto cit = category_map.find(info->getId());
                if (cit != category_map.end()) {
                    doc.CategoryIds = cit->second;
                }
                auto lit = label_map.find(info->getId());
                if (lit != label_map.end()) {
                    doc.LabelIds = lit->second;
                }

                batch_bytes +=
                    doc.Title.size() + doc.Content.size() + doc.CategoryIds.size() * sizeof(uint64_t) + doc.LabelIds.size() * sizeof(uint64_t);
                batch.push_back(std::move(doc));
                last_id = info->getId();

                if (batch_bytes >= batch_limit) {
                    if (!flush_batch()) {
                        return false;
                    }
                }
            }

            if (static_cast<int32_t>(infos.size()) < page_size) {
                break;
            }
        }

        if (!flush_batch()) {
            return false;
        }

        int64_t finished_at = static_cast<int64_t>(chen::TimeUtil::GetCurrentMs());
        int32_t commit_rt = client->callWithTimeout<int32_t>(
            rpc_timeout, static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_COMMIT), snapshot_id, finished_at);
        if (commit_rt != SEARCH_OK) {
            WARN(logger) << "search rebuild commit rejected, snapshot_id=" << snapshot_id << ", code=" << commit_rt;
            return false;
        }

        INFO(logger) << "search snapshot done, snapshot_id=" << snapshot_id << ", docs=" << total_docs;
        return true;
    } catch (const std::exception& e) {
        WARN(logger) << "search snapshot failed: " << e.what();
        invalidateClient();
        return false;
    }
}
