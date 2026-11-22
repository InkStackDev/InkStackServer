#include "ArticleCategoryRelManager.hpp"

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

DB::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::ArticleCategoryRelInfoDao::ParseRow(rt);
}

DB::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::ArticleCategoryRelInfoDao::Query(id, db);
}

bool ArticleCategoryRelManager::listByArticleId(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
    int64_t id,
    bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = DB::ArticleCategoryRelInfoDao::newQuery();
    qb->where("article_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (DB::ArticleCategoryRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

bool ArticleCategoryRelManager::listByCategoryId(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
    int64_t category_id,
    bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = DB::ArticleCategoryRelInfoDao::newQuery();
    qb->where("category_id", "=", category_id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (DB::ArticleCategoryRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

DB::ArticleCategoryRelInfo::ptr ArticleCategoryRelManager::getByArticleIdCategoryId(int64_t article_id,
    int64_t category_id)
{
    std::string ck = "acr:" + std::to_string(article_id) + ":" + std::to_string(category_id);
    int64_t cachedId = GetCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = DB::ArticleCategoryRelInfoDao::QueryByArticleIdCategoryId(article_id, category_id, db);
    if (info) {
        CacheIdMapping(ck, info->getId());
    }
    return info;
}
