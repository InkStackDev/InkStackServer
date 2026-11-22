#include "SearchClient.hpp"

#include "../Config/SearchConfig.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Socket/Address.hpp>
#include <Chen/Util/TimeUtil.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

void SearchClient::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (client_) {
        client_->close();
        client_.reset();
    }
    last_connect_ms_.store(0);
}

void SearchClient::Invalidate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (client_) {
        client_->close();
        client_.reset();
    }
}

chen::rpc::RpcClient::ptr SearchClient::GetClient()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return client_;
}

bool SearchClient::EnsureConnected()
{
    const SearchConf conf = GetSearchConf();
    std::lock_guard<std::mutex> lock(mutex_);
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
    INFO(logger) << "connected search hub: " << conf.hub_address;
    return true;
}

bool SearchClient::CheckStats(tagSearchStats& stats)
{
    auto client = GetClient();
    if (!client || !client->isConnected()) {
        return false;
    }

    try {
        stats = client->callWithTimeout<tagSearchStats>(
            GetSearchConf().timeout_ms, static_cast<uint32_t>(SS_CMD_SEARCH_GET_STATS));
        return stats.Code == SEARCH_OK;
    } catch (const std::exception& e) {
        WARN(logger) << "search stats failed: " << e.what();
        Invalidate();
        return false;
    }
}

bool SearchClient::Query(const tagSearchQuery& query, tagSearchResult& result)
{
    auto client = GetClient();
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
        Invalidate();
        return false;
    }
}

int32_t SearchClient::Upsert(const tagSearchDoc& doc)
{
    auto client = GetClient();
    if (!client || !client->isConnected()) {
        return -1;
    }

    try {
        int32_t rt = client->callWithTimeout<int32_t>(
            GetSearchConf().update_timeout_ms, static_cast<uint32_t>(SS_CMD_SEARCH_UPSERT_DOC), doc);
        if (rt != SEARCH_OK) {
            WARN(logger) << "upsert search doc rejected, article_id=" << doc.ArticleId << ", code=" << rt;
        }
        return rt;
    } catch (const std::exception& e) {
        WARN(logger) << "upsert search doc failed, article_id=" << doc.ArticleId << ": " << e.what();
        Invalidate();
        return -1;
    }
}

int32_t SearchClient::Remove(uint64_t article_id, int64_t update_time)
{
    auto client = GetClient();
    if (!client || !client->isConnected()) {
        return -1;
    }

    try {
        int32_t rt = client->callWithTimeout<int32_t>(GetSearchConf().update_timeout_ms,
            static_cast<uint32_t>(SS_CMD_SEARCH_REMOVE_ARTICLE), article_id, update_time);
        if (rt != SEARCH_OK) {
            WARN(logger) << "remove search article rejected, article_id=" << article_id << ", code=" << rt;
        }
        return rt;
    } catch (const std::exception& e) {
        WARN(logger) << "remove search article failed, article_id=" << article_id << ": " << e.what();
        Invalidate();
        return -1;
    }
}

int32_t SearchClient::RebuildBegin(uint64_t snapshot_id, int64_t started_at)
{
    auto client = GetClient();
    if (!client || !client->isConnected()) {
        return -1;
    }

    try {
        return client->callWithTimeout<int32_t>(GetSearchConf().snapshot_rpc_timeout_ms,
            static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_BEGIN), snapshot_id, started_at);
    } catch (const std::exception& e) {
        WARN(logger) << "search rebuild begin failed: " << e.what();
        Invalidate();
        return -1;
    }
}

int32_t SearchClient::RebuildDocs(uint64_t snapshot_id, const std::vector<tagSearchDoc>& docs)
{
    auto client = GetClient();
    if (!client || !client->isConnected()) {
        return -1;
    }

    try {
        return client->callWithTimeout<int32_t>(GetSearchConf().snapshot_rpc_timeout_ms,
            static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_DOCS), snapshot_id, docs);
    } catch (const std::exception& e) {
        WARN(logger) << "search rebuild docs failed: " << e.what();
        Invalidate();
        return -1;
    }
}

int32_t SearchClient::RebuildCommit(uint64_t snapshot_id, int64_t finished_at)
{
    auto client = GetClient();
    if (!client || !client->isConnected()) {
        return -1;
    }

    try {
        return client->callWithTimeout<int32_t>(GetSearchConf().snapshot_rpc_timeout_ms,
            static_cast<uint32_t>(SS_CMD_SEARCH_REBUILD_COMMIT), snapshot_id, finished_at);
    } catch (const std::exception& e) {
        WARN(logger) << "search rebuild commit failed: " << e.what();
        Invalidate();
        return -1;
    }
}
