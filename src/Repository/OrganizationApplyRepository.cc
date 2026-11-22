#include "OrganizationApplyRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"

namespace {

struct OrganizationApplyCodec {
    static DB::OrganizationApplyInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::OrganizationApplyInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setOrgName(CacheJsonString(v, "org_name"));
        info->setOrgType(CacheJsonString(v, "org_type"));
        info->setOrgDescription(CacheJsonString(v, "org_description"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setReviewReason(CacheJsonString(v, "review_reason"));
        info->setCreatedAt(CacheJsonTime(v, "created_at"));
        info->setReviewedAt(CacheJsonTime(v, "reviewed_at"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::OrganizationApplyInfo, OrganizationApplyCodec> g_apply_cache{{"org_apply", 60}};

} // namespace

DB::OrganizationApplyInfo::ptr OrganizationApplyRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_apply_cache.Get(id, [id, conn]() {
        return DB::OrganizationApplyInfoDao::Query(id, conn);
    });
}

bool OrganizationApplyRepository::Insert(DB::OrganizationApplyInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationApplyInfoDao::Insert(info, conn) == 0;
}

bool OrganizationApplyRepository::Update(DB::OrganizationApplyInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationApplyInfoDao::Update(info, conn) == 0;
}

void OrganizationApplyRepository::Put(const DB::OrganizationApplyInfo::ptr& info)
{
    g_apply_cache.Put(info);
}

void OrganizationApplyRepository::Evict(int64_t id)
{
    g_apply_cache.Evict(id);
}

int64_t OrganizationApplyRepository::ListByPages(std::vector<DB::OrganizationApplyInfo::ptr>& results,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationApplyInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("created_at", "DESC");

    int64_t total = 0;
    if (DB::OrganizationApplyInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)limit, conn)) {
        return 0;
    }
    return total;
}

int64_t OrganizationApplyRepository::ListByUser(std::vector<DB::OrganizationApplyInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t limit,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationApplyInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("user_id", "=", user_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("created_at", "DESC");

    int64_t total = 0;
    if (DB::OrganizationApplyInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)limit, conn)) {
        return 0;
    }
    return total;
}
