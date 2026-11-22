#include "ArticleLabelRelManager.hpp"

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

DB::ArticleLabelRelInfo::ptr ArticleLabelRelManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::ArticleLabelRelInfoDao::ParseRow(rt);
}

DB::ArticleLabelRelInfo::ptr ArticleLabelRelManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::ArticleLabelRelInfoDao::Query(id, db);
}

bool ArticleLabelRelManager::listByArticleId(std::vector<DB::ArticleLabelRelInfo::ptr>& infos, int64_t id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = DB::ArticleLabelRelInfoDao::newQuery();
    qb->where("article_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (DB::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

bool ArticleLabelRelManager::listByLabelId(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
    int64_t label_id,
    bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = DB::ArticleLabelRelInfoDao::newQuery();
    qb->where("label_id", "=", label_id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (DB::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

DB::ArticleLabelRelInfo::ptr ArticleLabelRelManager::getByArticleIdLabelId(int64_t article_id, int64_t label_id)
{
    std::string ck = "alr:" + std::to_string(article_id) + ":" + std::to_string(label_id);
    int64_t cachedId = GetCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = DB::ArticleLabelRelInfoDao::QueryByArticleIdLabelId(article_id, label_id, db);
    if (info) {
        CacheIdMapping(ck, info->getId());
    }
    return info;
}
