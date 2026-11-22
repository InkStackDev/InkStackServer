#include "TaskManager.hpp"

#include "../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

DB::TaskInfo::ptr TaskManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::TaskInfoDao::ParseRow(rt);
}

DB::TaskInfo::ptr TaskManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::TaskInfoDao::Query(id, db);
}

uint64_t TaskManager::listByPages(std::vector<DB::TaskInfo::ptr>& infos,
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
    auto qb = DB::TaskInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::TaskInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}

uint64_t TaskManager::listByOrg(std::vector<DB::TaskInfo::ptr>& infos,
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
    auto qb = DB::TaskInfoDao::newQuery();
    qb->where("org_id", "=", org_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::TaskInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}
