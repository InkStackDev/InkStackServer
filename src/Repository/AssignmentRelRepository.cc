#include "AssignmentRelRepository.hpp"

#include "../Cache/CacheUtil.hpp"

static std::string OrgAssignKey(int64_t org_id, int64_t assignment_id)
{
    return "assign_org:" + std::to_string(org_id) + ":" + std::to_string(assignment_id);
}

static std::string AssignUserKey(int64_t assignment_id, int64_t user_id)
{
    return "assign_usr:" + std::to_string(assignment_id) + ":" + std::to_string(user_id);
}

DB::AssignmentOrganizationRelInfo::ptr AssignmentOrganizationRelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::AssignmentOrganizationRelInfoDao::Query(id, conn);
}

DB::AssignmentOrganizationRelInfo::ptr AssignmentOrganizationRelRepository::GetByOrgAndAssign(int64_t org_id,
    int64_t assignment_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = OrgAssignKey(org_id, assignment_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::AssignmentOrganizationRelInfoDao::QueryByAssignmentIdOrganizationId(assignment_id, org_id, conn);
    if (info) {
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool AssignmentOrganizationRelRepository::InsertOrUpdate(DB::AssignmentOrganizationRelInfo::ptr info,
    chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::AssignmentOrganizationRelInfoDao::InsertOrUpdate(info, conn) == 0;
}

int64_t AssignmentOrganizationRelRepository::ListByAssignment(
    std::vector<DB::AssignmentOrganizationRelInfo::ptr>& results,
    int64_t assignment_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::AssignmentOrganizationRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("assignment_id", "=", assignment_id);
    qb->orderBy("id", "DESC");
    if (DB::AssignmentOrganizationRelInfoDao::QueryByBuilder(results, qb, conn)) {
        return 0;
    }
    return (int64_t)results.size();
}

int64_t AssignmentOrganizationRelRepository::ListByOrgPages(
    std::vector<DB::AssignmentOrganizationRelInfo::ptr>& results,
    int64_t org_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::AssignmentOrganizationRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("organization_id", "=", org_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::AssignmentOrganizationRelInfoDao::QueryByBuilderPages(
            results, total, qb, (int32_t)offset, (int32_t)size, conn)) {
        return 0;
    }
    return total;
}

DB::AssignmentUserRelInfo::ptr AssignmentUserRelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::AssignmentUserRelInfoDao::Query(id, conn);
}

DB::AssignmentUserRelInfo::ptr AssignmentUserRelRepository::GetByAssignAndUser(int64_t assignment_id,
    int64_t user_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = AssignUserKey(assignment_id, user_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::AssignmentUserRelInfoDao::QueryByAssignmentIdUserId(assignment_id, user_id, conn);
    if (info) {
        CacheIdMapping(key, info->getId());
    }
    return info;
}

int64_t AssignmentUserRelRepository::ListByAssignment(std::vector<DB::AssignmentUserRelInfo::ptr>& results,
    int64_t assignment_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::AssignmentUserRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("assignment_id", "=", assignment_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    if (DB::AssignmentUserRelInfoDao::QueryByBuilder(results, qb, conn)) {
        return 0;
    }
    return (int64_t)results.size();
}
