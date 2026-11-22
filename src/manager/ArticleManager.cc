#include "ArticleManager.hpp"

#include "../Search/SearchSync.hpp"
#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>

#include <ctime>
#include <set>

#include "../Config/AppConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:art:";
/// 浏览去重窗口（秒）
static const int kViewDedupTTL = 10 * 60;

static std::string ArticleCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static DB::ArticleInfo::ptr ParseArticleCache(const Json::Value& v)
{
    auto info = std::make_shared<DB::ArticleInfo>();
    info->setId(CacheJsonInt64(v, "id"));
    info->setUserId(CacheJsonInt64(v, "user_id"));
    info->setTitle(CacheJsonString(v, "title"));
    info->setContent(CacheJsonString(v, "content"));
    info->setType(CacheJsonInt32(v, "type"));
    info->setState(CacheJsonInt32(v, "state"));
    info->setChannel(CacheJsonInt64(v, "channel"));
    info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
    info->setPublishTime(CacheJsonTime(v, "publish_time"));
    info->setWeight(CacheJsonInt64(v, "weight"));
    info->setViews(CacheJsonInt64(v, "views"));
    info->setPraise(CacheJsonInt64(v, "praise"));
    info->setFavorites(CacheJsonInt64(v, "favorites"));
    info->setCreateTime(CacheJsonTime(v, "create_time"));
    info->setUpdateTime(CacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static DB::ArticleInfo::ptr GetArticleCache(int64_t id)
{
    std::string key = ArticleCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseArticleCache(v);
}

static void SetArticleCache(const DB::ArticleInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(ArticleCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

static void DelArticleCache(int64_t id)
{
    RedisCacheDel(ArticleCacheKey(id));
}

DB::ArticleInfo::ptr ArticleManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::ArticleInfoDao::ParseRow(rt);
}

void ArticleManager::add(DB::ArticleInfo::ptr info)
{
    SetArticleCache(info);
}

void ArticleManager::del(int64_t id)
{
    DelArticleCache(id);
}

DB::ArticleInfo::ptr ArticleManager::get(int64_t id)
{
    auto v = GetArticleCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = DB::ArticleInfoDao::Query(id, db);
    if (v) {
        SetArticleCache(v);
    }
    return v;
}

bool ArticleManager::listByUserId(std::vector<DB::ArticleInfo::ptr>& infos, int64_t id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    qb->where("user_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (DB::ArticleInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

int64_t ArticleManager::listByUserIdPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t id,
    int32_t offset,
    int32_t size,
    bool valid,
    int state)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    qb->whereIf(id != 0, "user_id", "=", id);
    qb->whereIf(state != 0, "state", "=", (int64_t)state);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    return total;
}

int64_t ArticleManager::listByLabelPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t label_id,
    int32_t offset,
    int32_t size,
    bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery("a");
    qb->select(
        "a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, a.weight, a.views, a.praise, a.favorites, "
        "a.create_time, a.update_time");
    qb->join("article_label_rel alr", "a.id = alr.article_id");
    qb->where("alr.label_id", "=", label_id);
    qb->where("alr.is_deleted", "=", (int64_t)0);
    qb->where("a.state", "=", (int64_t)PUBLISHED);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    qb->orderBy("a.id", "DESC");

    // 结果缓存：仅对首页做缓存
    std::string listKey =
        "art:lbl:" + std::to_string(label_id) + ":" + (valid ? "1" : "0") + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (GetCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                infos.push_back(info);
            }
        }
        return ExecuteCountCached(qb, db, "art:lbl:" + std::to_string(label_id) + ":" + (valid ? "1" : "0"));
    }

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    std::vector<int64_t> ids;
    for (auto& info : infos) {
        ids.push_back(info->getId());
    }
    CacheListResult(listKey, ids);
    return total;
}

int64_t ArticleManager::listByCategoryPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int64_t category_id,
    int32_t offset,
    int32_t size,
    bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery("a");
    qb->select(
        "a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, a.weight, a.views, a.praise, a.favorites, "
        "a.create_time, a.update_time");
    qb->join("article_category_rel acr", "a.id = acr.article_id");
    qb->where("acr.category_id", "=", category_id);
    qb->where("acr.is_deleted", "=", (int64_t)0);
    qb->where("a.state", "=", (int64_t)PUBLISHED);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    qb->orderBy("a.id", "DESC");

    // 结果缓存：仅对首页做缓存
    std::string listKey =
        "art:cat:" + std::to_string(category_id) + ":" + (valid ? "1" : "0") + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (GetCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                infos.push_back(info);
            }
        }
        return ExecuteCountCached(qb, db, "art:cat:" + std::to_string(category_id) + ":" + (valid ? "1" : "0"));
    }

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    std::vector<int64_t> ids;
    for (auto& info : infos) {
        ids.push_back(info->getId());
    }
    CacheListResult(listKey, ids);
    return total;
}

int64_t ArticleManager::listByPages(std::vector<DB::ArticleInfo::ptr>& infos,
    int32_t offset,
    int state,
    int category,
    int32_t role,
    int32_t days,
    int32_t size,
    bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = DB::ArticleInfoDao::newQuery("a");
    qb->select(
        "a.id, a.user_id, a.title, a.content, a.type, a.state, a.channel, a.is_deleted, a.publish_time, a.weight, a.views, a.praise, a.favorites, "
        "a.create_time, a.update_time");

    if (category > 0) {
        qb->join("article_category_rel acr", "a.id = acr.article_id");
        qb->where("acr.category_id", "=", (int64_t)category);
        qb->where("acr.is_deleted", "=", (int64_t)0);
    }
    if (role != -1) {
        qb->join("user u", "a.user_id = u.id");
        qb->where("u.role", "=", (int64_t)role);
        qb->where("u.is_deleted", "=", (int64_t)0);
    }
    qb->whereIf(state != 0, "a.state", "=", (int64_t)state);
    qb->whereIf(valid, "a.is_deleted", "=", (int64_t)0);
    if (days > 0) {
        time_t now = time(0);
        qb->whereSQL("a.create_time >= ?", (int64_t)(now - days * 24 * 3600));
    }
    qb->orderBy("a.id", "DESC");

    {
        std::stringstream ck;
        ck << "art:list:" << state << ":" << category << ":" << role << ":" << days << ":" << (valid ? "1" : "0");
        int64_t total = 0;
        if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
            return 0;
        }
        return total;
    }
}

void ArticleManager::delVerify(int64_t id)
{
    DelArticleCache(id);
}

void ArticleManager::addVerify(DB::ArticleInfo::ptr info)
{
    SetArticleCache(info);
}

int64_t ArticleManager::listVerifyPages(std::vector<DB::ArticleInfo::ptr>& infos, int32_t offset, int32_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    qb->where("state", "=", (int64_t)CHECKING);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::ArticleInfoDao::QueryByBuilderPages(infos, total, qb, offset, size, db)) {
        return 0;
    }
    for (auto& info : infos) {
        if (info->getIsDeleted()) {
            DelArticleCache(info->getId());
            continue;
        }
    }
    return total;
}

std::pair<DB::ArticleInfo::ptr, DB::ArticleInfo::ptr> ArticleManager::nearby(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return std::pair(nullptr, nullptr);
    }

    // 查找上一篇（id 最大的小于给定 id 的已发布文章）
    DB::ArticleInfo::ptr prev;
    {
        auto qb = DB::ArticleInfoDao::newQuery();
        qb->whereSQL("id < ?", id);
        qb->where("state", "=", (int64_t)PUBLISHED);
        qb->where("is_deleted", "=", (int64_t)0);
        qb->orderBy("id", "DESC");
        qb->limit(1);
        std::vector<DB::ArticleInfo::ptr> results;
        if (DB::ArticleInfoDao::QueryByBuilder(results, qb, db) == 0 && !results.empty()) {
            prev = results[0];
        }
    }

    // 查找下一篇（id 最小的大于给定 id 的已发布文章）
    DB::ArticleInfo::ptr next;
    {
        auto qb = DB::ArticleInfoDao::newQuery();
        qb->whereSQL("id > ?", id);
        qb->where("state", "=", (int64_t)PUBLISHED);
        qb->where("is_deleted", "=", (int64_t)0);
        qb->orderBy("id", "ASC");
        qb->limit(1);
        std::vector<DB::ArticleInfo::ptr> results;
        if (DB::ArticleInfoDao::QueryByBuilder(results, qb, db) == 0 && !results.empty()) {
            next = results[0];
        }
    }

    return std::pair(prev, next);
}

ArticleManager::ArticleStats ArticleManager::getStats(int32_t category, int32_t role, int32_t days, const std::string& keyword)
{
    ArticleStats stats;

    // 无过滤条件时走缓存
    if (category <= 0 && role == -1 && days <= 0 && keyword.empty()) {
        std::string cached;
        if (GetCachedStringResult("art:stats", cached)) {
            std::stringstream ss(cached);
            std::string token;
            auto next = [&]() -> int64_t {
                std::getline(ss, token, '|');
                return chen::TypeUtil::Atoi(token);
            };
            stats.total = next();
            stats.pending = next();
            stats.published = next();
            stats.rejected = next();
            stats.reported = next();
            return stats;
        }
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    auto qb = DB::ArticleInfoDao::newQuery("a");
    qb->select("a.state, COUNT(*) as cnt");

    if (category > 0) {
        qb->join("article_category_rel acr", "a.id = acr.article_id");
        qb->where("acr.category_id", "=", (int64_t)category);
        qb->where("acr.is_deleted", "=", (int64_t)0);
    }
    if (role != -1) {
        qb->join("user u", "a.user_id = u.id");
        qb->where("u.role", "=", (int64_t)role);
        qb->where("u.is_deleted", "=", (int64_t)0);
    }
    qb->where("a.is_deleted", "=", (int64_t)0);
    if (days > 0) {
        time_t now = time(0);
        qb->whereSQL("a.create_time >= ?", (int64_t)(now - days * 24 * 3600));
    }
    if (!keyword.empty()) {
        qb->where("a.title", "LIKE", "%" + keyword + "%");
    }
    qb->groupBy("a.state");

    std::vector<std::pair<int32_t, int64_t>> rows;
    if (qb->queryPairs<int32_t, int64_t>(rows, db)) {
        return stats;
    }
    for (auto& [st, cnt] : rows) {
        stats.total += cnt;
        switch (st) {
        case CHECKING:
            stats.pending += cnt;
            break;
        case PUBLISHED:
            stats.published += cnt;
            break;
        case REJECTED:
            stats.rejected += cnt;
            break;
        default:
            break;
        }
    }

    // 无过滤条件时缓存结果
    if (category <= 0 && role == -1 && days <= 0 && keyword.empty()) {
        std::stringstream ss;
        ss << stats.total << "|" << stats.pending << "|" << stats.published << "|" << stats.rejected << "|" << stats.reported;
        CacheStringResult("art:stats", ss.str());
    }

    return stats;
}

std::string ArticleManager::statusString()
{
    std::stringstream ss;
    ss << "ArticleManager cache=redis prefix=" << kCachePrefix << " ttl=" << kCacheTTL;
    return ss.str();
}

void ArticleManager::start()
{
    syncStatsFromDB();
}

void ArticleManager::stop()
{
    // 停止前将脏数据（浏览/点赞/收藏数）刷新到 DB，避免 reload/stop 时丢失
    flushDirty();
}

void ArticleManager::flushDirty()
{
    std::set<int64_t> updates;
    {
        std::unique_lock<std::shared_mutex> lock(updates_mutex_);
        updates.swap(updates_);
    }

    if (updates.empty()) {
        return;
    }
    auto conn = GetDB();
    if (!conn) {
        ERROR(logger) << "flushDirty: get db connect fail";

        std::unique_lock<std::shared_mutex> lock(updates_mutex_);
        for (auto& i : updates) {
            updates_.insert(i);
        }
        return;
    }
    auto trans = conn->openTransaction();
    if (!trans) {
        ERROR(logger) << "flushDirty: open transaction failed";
        std::unique_lock<std::shared_mutex> lock(updates_mutex_);
        for (auto& i : updates) {
            updates_.insert(i);
        }
        return;
    }
    std::vector<DB::ArticleInfo::ptr> updated;
    for (auto& i : updates) {
        auto info = get(i);
        if (!info) {
            continue;
        }
        // Redis 反序列化后对象是 clean 的，这里显式标记计数字段，确保能写回 DB
        info->setViews(info->getViews());
        info->setPraise(info->getPraise());
        info->setFavorites(info->getFavorites());
        if (DB::ArticleInfoDao::Update(info, conn)) {
            addUpdate(i);
        } else {
            updated.push_back(info);
        }
    }
    if (!trans->commit()) {
        ERROR(logger) << "flushDirty: commit failed";
        std::unique_lock<std::shared_mutex> lock(updates_mutex_);
        for (auto& i : updates) {
            updates_.insert(i);
        }
        return;
    }
    for (const auto& info : updated) {
        SetArticleCache(info);
    }
    INFO(logger) << "flushDirty: flushed " << updates.size() << " articles";
}

bool ArticleManager::incViews(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(counter_mutex_);
    auto info = get(id);
    if (!info) {
        return false;
    }
    bool v = addViews(id, cookie_id);
    if (v) {
        info->setViews(info->getViews() + 1);
        SetArticleCache(info);
        addUpdate(id);
        chen::IOManager::GetThis()->schedule([user_id]() {
            chen::RedisUtil::Cmd(GetRedisPoolName(), "incr blog:total_visits");
            auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "incr blog:today_visits");
            if (rpy && rpy->integer == 1) {
                int64_t now = chen::TimeUtil::GetCurrentSec();
                int64_t tomorrow_midnight = chen::TimeUtil::GetDayStart(now + 86400);
                chen::RedisUtil::Cmd(GetRedisPoolName(), "expireat blog:today_visits %lld", tomorrow_midnight);
            }
            chen::RedisUtil::Cmd(GetRedisPoolName(), "pfadd blog:visitors %lld", user_id);
        });
    }
    return true;
}

bool ArticleManager::incPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(counter_mutex_);
    auto info = get(id);
    if (!info) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hexist pra_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hexists fail";
        return false;
    }
    if (rpy->integer == 1) {
        return true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hset pra_a2u:%lld %lld %lld", id, user_id, time(0));
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    chen::IOManager::GetThis()->schedule([id, user_id]() {
        chen::RedisUtil::Cmd(GetRedisPoolName(), "hset pra_u2a:%lld %lld %lld", user_id, id, time(0));
    });
    info->setPraise(info->getPraise() + 1);
    SetArticleCache(info);
    addUpdate(id);

    return true;
}

bool ArticleManager::incFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(counter_mutex_);
    auto info = get(id);
    if (!info) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hexist fav_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hexists fail";
        return false;
    }
    if (rpy->integer == 1) {
        return true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hset fav_a2u:%lld %lld %lld", id, user_id, time(0));
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    chen::IOManager::GetThis()->schedule([id, user_id]() {
        chen::RedisUtil::Cmd(GetRedisPoolName(), "hset fav_u2a:%lld %lld %lld", user_id, id, time(0));
    });
    info->setFavorites(info->getFavorites() + 1);
    SetArticleCache(info);
    addUpdate(id);

    return true;
}

bool ArticleManager::decPraise(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(counter_mutex_);
    auto info = get(id);
    if (!info) {
        return false;
    }
    bool v = false;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hdel pra_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hdel pra_u2a:%lld %lld", user_id, id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    if (v) {
        info->setPraise(info->getPraise() - 1);
        SetArticleCache(info);
        addUpdate(id);
    }

    return true;
}

bool ArticleManager::decFavorites(uint64_t id, const std::string& cookie_id, uint64_t user_id)
{
    std::lock_guard<std::mutex> lock(counter_mutex_);
    auto info = get(id);
    if (!info) {
        return false;
    }
    bool v = false;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hdel fav_a2u:%lld %lld", id, user_id);
    if (!rpy) {
        ERROR(logger) << "hdel fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "hset fav_u2a:%lld %lld", user_id, id);
    if (!rpy) {
        ERROR(logger) << "hset fail";
        return false;
    }
    if (rpy->integer == 1) {
        v = true;
    }
    if (v) {
        info->setFavorites(info->getFavorites() - 1);
        SetArticleCache(info);
        addUpdate(id);
    }

    return true;
}

bool ArticleManager::listUserFav(int64_t id, std::map<int64_t, int64_t>& articles)
{
#define PROC(id, mask, articles)                                                                                       \
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), mask, id);                                                     \
    if (!rpy) {                                                                                                \
        ERROR(logger) << "hgetall fail";                                                                       \
        return false;                                                                                          \
    }                                                                                                          \
    for (size_t i = 0; i < rpy->elements; i += 2) {                                                            \
        articles[chen::TypeUtil::Atoi(rpy->element[i]->str)] = chen::TypeUtil::Atoi(rpy->element[i + 1]->str); \
    }                                                                                                          \
    return true;

    PROC(id, "hgetall fav_u2a:%lld", articles);
}

bool ArticleManager::listUserPra(int64_t id, std::map<int64_t, int64_t>& articles)
{
    PROC(id, "hgetall pra_u2a:%lld", articles);
}

bool ArticleManager::listArticleFav(int64_t id, std::map<int64_t, int64_t>& users)
{
    PROC(id, "hgetall fav_a2u:%lld", users);
}

bool ArticleManager::listArticlePra(int64_t id, std::map<int64_t, int64_t>& users)
{
    PROC(id, "hgetall pra_a2u:%lld", users);
}
#undef PROC

static const char* kScheduleKey = "article:schedule";

void ArticleManager::scheduleArticle(int64_t article_id, int64_t publish_time)
{
    chen::RedisUtil::Cmd(
        GetRedisPoolName(), "zadd %s %lld %lld", kScheduleKey, (long long)publish_time, (long long)article_id);
}

void ArticleManager::unscheduleArticle(int64_t article_id)
{
    chen::RedisUtil::Cmd(GetRedisPoolName(), "zrem %s %lld", kScheduleKey, (long long)article_id);
}

void ArticleManager::onTick()
{
    doArticlePublishSchedule();

    flushDirty();
}

bool ArticleManager::addViews(uint64_t id, const std::string& cookie_id)
{
    // 用 Redis SET NX EX 做 10 分钟去重，避免进程内缓存无界增长
    std::string key = "art:view:" + std::to_string(id) + ":" + cookie_id;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "set %s 1 ex %d nx", key.c_str(), kViewDedupTTL);
    if (!rpy) {
        // Redis 异常时不去重，避免丢掉有效浏览
        return true;
    }
    // NX 未命中返回 nil，说明该用户 10 分钟内已经浏览过
    return rpy->type != REDIS_REPLY_NIL;
}

void ArticleManager::addUpdate(int64_t id)
{
    std::unique_lock<std::shared_mutex> lock(updates_mutex_);
    updates_.insert(id);
}

void ArticleManager::incPraiseCount(int64_t id)
{
    std::lock_guard<std::mutex> lock(counter_mutex_);
    auto info = get(id);
    if (!info) {
        return;
    }
    info->setPraise(info->getPraise() + 1);
    SetArticleCache(info);
    addUpdate(id);
}

void ArticleManager::decPraiseCount(int64_t id)
{
    std::lock_guard<std::mutex> lock(counter_mutex_);
    auto info = get(id);
    if (!info) {
        return;
    }
    if (info->getPraise() > 0) {
        info->setPraise(info->getPraise() - 1);
        SetArticleCache(info);
        addUpdate(id);
    }
}

int64_t ArticleManager::getTodayViews()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get blog:today_visits");
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    return 0;
}

int64_t ArticleManager::getTotalViews()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get blog:total_visits");
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    // Redis 未初始化 — 从 DB 计算并回填
    auto db = GetDB();
    if (!db) {
        return 0;
    }
    auto qb = DB::ArticleInfoDao::newQuery();
    qb->select("CAST(COALESCE(SUM(views), 0) AS SIGNED)");
    qb->where("state", "=", (int64_t)PUBLISHED);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    qb->queryScalarInt64(total, db);
    chen::RedisUtil::Cmd(GetRedisPoolName(), "set blog:total_visits %lld", total);
    return total;
}

int64_t ArticleManager::getTotalVisitors()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "pfcount blog:visitors");
    if (rpy) {
        return rpy->integer;
    }
    return 0;
}

void ArticleManager::syncStatsFromDB()
{
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del blog:total_visits");
    int64_t total = getTotalViews();
    INFO(logger) << "syncStatsFromDB: blog:total_visits recalculated from DB = " << total;
}

void ArticleManager::getCalendarDays(int64_t user_id, int32_t year, int32_t month, std::vector<int32_t>& days)
{
    // 先查 Redis 缓存
    {
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "GET calendar:%lld:%d:%d", user_id, year, month);
        if (rpy && rpy->str && strlen(rpy->str) > 0) {
            Json::Value cached;
            if (chen::JsonUtil::FromString(cached, rpy->str) && cached.isArray() && cached.size() > 0) {
                for (auto& v : cached) {
                    days.push_back(v.asInt());
                }
                return;
            }
        }
    }

    // 缓存未命中：查数据库
    // 直接用日期字符串比较，避免 mktime/localtime_r 与 MySQL UNIX_TIMESTAMP() 之间
    // 的时区不一致问题（前者用服务器本地时区，后者用 MySQL session 时区）
    char start_str[32], end_str[32];
    snprintf(start_str, sizeof(start_str), "%04d-%02d-01 00:00:00", year, month);
    int next_month = month + 1;
    int next_year = year;
    if (next_month > 12) {
        next_month = 1;
        next_year++;
    }
    snprintf(end_str, sizeof(end_str), "%04d-%02d-01 00:00:00", next_year, next_month);

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getCalendarDays: Get DB connection fail";
        return;
    }

    auto qb = DB::ArticleInfoDao::newQuery();
    qb->select("DISTINCT DAY(publish_time) as d");
    qb->where("user_id", "=", user_id);
    qb->where("state", "=", (int64_t)PUBLISHED);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("publish_time", ">=", std::string(start_str));
    qb->where("publish_time", "<", std::string(end_str));
    qb->orderBy("d", "ASC");

    qb->queryColumn<int32_t>(days, db, "d");

    // 写入 Redis 缓存（TTL 5 分钟），空结果不缓存，避免因首次查询无数据而导致
    // 后续文章发布后仍返回空
    if (!days.empty()) {
        Json::Value daysJson(Json::arrayValue);
        for (auto d : days) {
            daysJson.append(d);
        }
        std::string jsonStr = chen::JsonUtil::ToString(daysJson);
        chen::RedisUtil::Cmd(
            GetRedisPoolName(), "SETEX calendar:%lld:%d:%d 300 %s", user_id, year, month, jsonStr.c_str());
    }
}

void ArticleManager::clearCalendarCache(int64_t user_id, int64_t publish_time)
{
    time_t pt = static_cast<time_t>(publish_time);
    struct tm tm_pt = {};
    localtime_r(&pt, &tm_pt);
    int32_t year = tm_pt.tm_year + 1900;
    int32_t month = tm_pt.tm_mon + 1;
    chen::RedisUtil::Cmd(GetRedisPoolName(), "DEL calendar:%lld:%d:%d", user_id, year, month);
}

void ArticleManager::doArticlePublishSchedule()
{
    time_t now = chen::TimeUtil::GetCurrentSec();

    // 从 Redis sorted set 获取已到期的文章 ID
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "zrangebyscore %s 0 %lld", kScheduleKey, now);
    if (!rpy || rpy->type != REDIS_REPLY_ARRAY || rpy->elements == 0) {
        return;
    }

    // 收集需要发布的文章 ID
    std::vector<int64_t> article_ids;
    for (size_t i = 0; i < rpy->elements; ++i) {
        if (rpy->element[i]->type == REDIS_REPLY_STRING && rpy->element[i]->str) {
            article_ids.push_back(chen::TypeUtil::Atoi(rpy->element[i]->str));
        }
    }

    if (article_ids.empty()) {
        return;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getDB error";
        return;
    }

    // 批量从 sorted set 中移除（原子操作）
    std::string zrem_cmd = "zrem " + std::string(kScheduleKey);
    for (auto id : article_ids) {
        zrem_cmd += " " + std::to_string(id);
    }
    chen::RedisUtil::Cmd(GetRedisPoolName(), zrem_cmd.c_str());

    // 逐个更新文章状态
    std::vector<int64_t> published_ids;
    std::vector<DB::ArticleInfo::ptr> published;
    auto trans = db->openTransaction();
    if (!trans) {
        ERROR(logger) << "open transaction failed for scheduled publish";
        return;
    }
    for (auto id : article_ids) {
        auto info = get(id);
        if (!info || info->getIsDeleted()) {
            continue;
        }
        if (info->getState() == PUBLISHED) {
            continue;
        }

        info->setState(PUBLISHED);
        info->setUpdateTime(now);

        if (DB::ArticleInfoDao::Update(info, db)) {
            ERROR(logger) << "Update error errno=" << db->getErrno() << " errstr=" << db->getErrStr() << " data=" << info->toJsonString();
            continue;
        }
        published.push_back(info);
        published_ids.push_back(info->getId());
    }
    if (!trans->commit()) {
        ERROR(logger) << "commit scheduled publish failed";
        return;
    }
    for (const auto& info : published) {
        SetArticleCache(info);
        clearCalendarCache(info->getUserId(), info->getPublishTime());
    }
    for (auto id : published_ids) {
        SearchSyncMgr::GetInstance()->markDirty(id, SearchSync::OP_UPSERT);
    }
}
