#include "OrganizationApplyManager.hpp"

#include <Chen/Log/Log.hpp>

#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::OrganizationApplyInfo::ptr OrganizationApplyManager::ParseRow(chen::ISQLData::ptr rt)
{
    return DB::OrganizationApplyInfoDao::ParseRow(rt);
}

DB::OrganizationApplyInfo::ptr OrganizationApplyManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return DB::OrganizationApplyInfoDao::Query(id, db);
}

int64_t OrganizationApplyManager::listByPages(std::vector<DB::OrganizationApplyInfo::ptr>& results,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool is_valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::OrganizationApplyInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("created_at", "DESC");

    int64_t total = 0;
    if (DB::OrganizationApplyInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        return 0;
    }
    return total;
}

int64_t OrganizationApplyManager::listByUserId(std::vector<DB::OrganizationApplyInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t limit,
    bool is_valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = DB::OrganizationApplyInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("created_at", "DESC");

    int64_t total = 0;
    if (DB::OrganizationApplyInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        return 0;
    }
    return total;
}
