#include "BugManager.hpp"

#include <Chen/Log/Log.hpp>

#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::BugInfo::ptr BugManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::BugInfoDao::ParseRow(rt);
}

DB::BugInfo::ptr BugManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::BugInfoDao::Query(id, db);
}

uint64_t BugManager::listByPages(std::vector<DB::BugInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::BugInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::BugInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}

uint64_t BugManager::listByOrg(std::vector<DB::BugInfo::ptr>& infos,
    int64_t org_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::BugInfoDao::newQuery();
    qb->where("org_id", "=", org_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::BugInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}
