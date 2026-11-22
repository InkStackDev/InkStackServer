#include "ArticlePraiseRelManager.hpp"

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:apra:";

static std::string ArticlePraiseRelCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static DB::ArticlePraiseRelInfo::ptr ParseArticlePraiseRelCache(const Json::Value& v)
{
    auto info = std::make_shared<DB::ArticlePraiseRelInfo>();
    info->setId(CacheJsonInt64(v, "id"));
    info->setUserId(CacheJsonInt64(v, "user_id"));
    info->setArticleId(CacheJsonInt64(v, "article_id"));
    info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(CacheJsonTime(v, "create_time"));
    info->setUpdateTime(CacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static DB::ArticlePraiseRelInfo::ptr GetArticlePraiseRelCache(int64_t id)
{
    std::string key = ArticlePraiseRelCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseArticlePraiseRelCache(v);
}

static void SetArticlePraiseRelCache(const DB::ArticlePraiseRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(ArticlePraiseRelCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::ArticlePraiseRelInfoDao::ParseRow(rt);
}

void ArticlePraiseRelManager::add(DB::ArticlePraiseRelInfo::ptr info)
{
    SetArticlePraiseRelCache(info);
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::get(int64_t id)
{
    auto v = GetArticlePraiseRelCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = DB::ArticlePraiseRelInfoDao::Query(id, db);
    if (v) {
        SetArticlePraiseRelCache(v);
    }
    return v;
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::getByUserAndArticle(int64_t user_id, int64_t article_id)
{
    std::string ck = "pra:" + std::to_string(user_id) + ":" + std::to_string(article_id);
    int64_t cachedId = GetCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = DB::ArticlePraiseRelInfoDao::QueryByUserIdArticleId(user_id, article_id, db);
    if (info) {
        SetArticlePraiseRelCache(info);
        CacheIdMapping(ck, info->getId());
    }
    return info;
}

DB::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::praise(int64_t user_id, int64_t article_id)
{
    auto existing = getByUserAndArticle(user_id, article_id);
    if (existing) {
        if (!existing->getIsDeleted()) {
            return existing;
        }
        auto db = GetDB();
        if (!db) {
            ERROR(logger) << "Get DB connection fail";
            return nullptr;
        }
        existing->setIsDeleted(0);
        existing->setUpdateTime(time(0));
        if (DB::ArticlePraiseRelInfoDao::Update(existing, db)) {
            ERROR(logger) << "ArticlePraiseRelManager praise Update fail";
            return nullptr;
        }
        SetArticlePraiseRelCache(existing);
        return existing;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<DB::ArticlePraiseRelInfo>();
    info->setUserId(user_id);
    info->setArticleId(article_id);
    info->setIsDeleted(0);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (DB::ArticlePraiseRelInfoDao::Insert(info, db)) {
        ERROR(logger) << "ArticlePraiseRelManager praise Insert fail";
        return nullptr;
    }

    SetArticlePraiseRelCache(info);
    return info;
}

bool ArticlePraiseRelManager::unpraise(int64_t user_id, int64_t article_id)
{
    auto info = getByUserAndArticle(user_id, article_id);
    if (!info || info->getIsDeleted()) {
        return false;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(time(0));
    if (DB::ArticlePraiseRelInfoDao::Update(info, db)) {
        ERROR(logger) << "ArticlePraiseRelManager unpraise Update fail";
        return false;
    }
    SetArticlePraiseRelCache(info);
    return true;
}

bool ArticlePraiseRelManager::isPraising(int64_t user_id, int64_t article_id)
{
    auto info = getByUserAndArticle(user_id, article_id);
    return info && !info->getIsDeleted();
}

void ArticlePraiseRelManager::listByArticle(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
    int64_t article_id,
    uint64_t offset,
    uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (DB::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

void ArticlePraiseRelManager::listByUser(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (DB::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
}

int64_t ArticlePraiseRelManager::countByArticle(int64_t article_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

int64_t ArticlePraiseRelManager::countByUser(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::ArticlePraiseRelInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}
