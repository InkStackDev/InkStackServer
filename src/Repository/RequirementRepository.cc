#include "RequirementRepository.hpp"

DB::RequirementInfo::ptr RequirementRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::RequirementInfoDao::Query(id, conn);
}

bool RequirementRepository::Insert(DB::RequirementInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::RequirementInfoDao::Insert(info, conn) == 0;
}

bool RequirementRepository::InsertOrUpdate(DB::RequirementInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::RequirementInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool RequirementRepository::Update(DB::RequirementInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::RequirementInfoDao::Update(info, conn) == 0;
}

uint64_t RequirementRepository::ListByPages(std::vector<DB::RequirementInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::RequirementInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::RequirementInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, conn)) {
        return 0;
    }
    return (uint64_t)total;
}

uint64_t RequirementRepository::ListByOrg(std::vector<DB::RequirementInfo::ptr>& infos,
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
    auto qb = DB::RequirementInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("org_id", "=", org_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::RequirementInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, conn)) {
        return 0;
    }
    return (uint64_t)total;
}
