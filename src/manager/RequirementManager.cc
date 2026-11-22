#include "RequirementManager.hpp"

#include "../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

DB::RequirementInfo::ptr RequirementManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::RequirementInfoDao::ParseRow(rt);
}

DB::RequirementInfo::ptr RequirementManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::RequirementInfoDao::Query(id, db);
}

uint64_t RequirementManager::listByPages(std::vector<DB::RequirementInfo::ptr>& infos,
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
    auto qb = DB::RequirementInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::RequirementInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}

uint64_t RequirementManager::listByOrg(std::vector<DB::RequirementInfo::ptr>& infos,
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
    auto qb = DB::RequirementInfoDao::newQuery();
    qb->where("org_id", "=", org_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::RequirementInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}
