#include "OrganizationUserRelRepository.hpp"

#include <memory>
#include <utility>

#include <Chen/DB/Redis.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

namespace {

constexpr int32_t kStatusPending = 0;
constexpr int32_t kStatusApproved = 1;
constexpr int32_t kStatusExited = 3;
constexpr int32_t kStatusRejected = 2;
constexpr int32_t kRoleMember = 1;
constexpr int32_t kRoleOrgAdmin = 5;

struct OrgUserRelCodec {
    static DB::OrganizationUserRelInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::OrganizationUserRelInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setOrgId(CacheJsonInt64(v, "org_id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setRole(CacheJsonInt32(v, "role"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::OrganizationUserRelInfo, OrgUserRelCodec> g_org_user_cache{{"org_usr", 120}};

std::string OrgUserKey(int64_t org_id, int64_t user_id)
{
    return "org_usr:" + std::to_string(org_id) + ":" + std::to_string(user_id);
}

} // namespace

DB::OrganizationUserRelInfo::ptr OrganizationUserRelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_org_user_cache.Get(id, [id, conn]() {
        return DB::OrganizationUserRelInfoDao::Query(id, conn);
    });
}

DB::OrganizationUserRelInfo::ptr OrganizationUserRelRepository::GetByOrgAndUser(int64_t org_id,
    int64_t user_id,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = OrgUserKey(org_id, user_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::OrganizationUserRelInfoDao::QueryByOrgIdUserId(org_id, user_id, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool OrganizationUserRelRepository::InsertOrUpdate(DB::OrganizationUserRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationUserRelInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool OrganizationUserRelRepository::Update(DB::OrganizationUserRelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationUserRelInfoDao::Update(info, conn) == 0;
}

void OrganizationUserRelRepository::Put(const DB::OrganizationUserRelInfo::ptr& info)
{
    if (!info) {
        return;
    }
    g_org_user_cache.Put(info);
    CacheIdMapping(OrgUserKey(info->getOrgId(), info->getUserId()), info->getId());
}

void OrganizationUserRelRepository::Evict(int64_t id)
{
    g_org_user_cache.Evict(id);
}

int64_t OrganizationUserRelRepository::ListByOrgPages(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
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
    auto qb = DB::OrganizationUserRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("org_id", "=", org_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::OrganizationUserRelInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)size, conn)) {
        return 0;
    }
    return total;
}

int64_t OrganizationUserRelRepository::ListByUser(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
    int64_t user_id,
    int32_t status,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationUserRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("user_id", "=", user_id);
    qb->where("status", "!=", (int64_t)kStatusPending);
    qb->where("status", "!=", (int64_t)kStatusExited);
    qb->where("status", "!=", (int64_t)kStatusRejected);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (DB::OrganizationUserRelInfoDao::QueryByBuilder(results, qb, conn)) {
        return 0;
    }
    return (int64_t)results.size();
}

int64_t OrganizationUserRelRepository::ListByOrg(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
    int64_t org_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationUserRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("org_id", "=", org_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    if (DB::OrganizationUserRelInfoDao::QueryByBuilder(results, qb, conn)) {
        return 0;
    }
    return (int64_t)results.size();
}

int64_t OrganizationUserRelRepository::CountByOrg(int64_t org_id, int32_t status, bool is_valid, chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationUserRelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("org_id", "=", org_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}

OrganizationUserRelRepository::Stats OrganizationUserRelRepository::GetStats(int64_t org_id, chen::IDB::ptr conn)
{
    Stats stats;
    if (!conn) {
        return stats;
    }

    stats.total_members = CountByOrg(org_id, kStatusApproved, true, conn);

    {
        auto qb = DB::OrganizationUserRelInfoDao::newQuery();
        if (qb) {
            qb->select("role, COUNT(*) as cnt");
            qb->where("org_id", "=", org_id);
            qb->where("status", "=", (int64_t)kStatusApproved);
            qb->where("is_deleted", "=", (int64_t)0);
            qb->groupBy("role");
            std::vector<std::pair<int32_t, int64_t>> rows;
            if (qb->queryPairs<int32_t, int64_t>(rows, conn) == 0) {
                for (auto& [role, count] : rows) {
                    if (role == kRoleOrgAdmin) {
                        stats.org_admin_count = count;
                    } else if (role == kRoleMember) {
                        stats.regular_count = count;
                    }
                }
            }
        }
    }

    {
        auto qb = DB::OrganizationUserRelInfoDao::newQuery("r");
        if (qb) {
            qb->join("INNER", "user u", "r.user_id = u.id");
            qb->where("r.org_id", "=", org_id);
            qb->where("r.status", "=", (int64_t)kStatusApproved);
            qb->where("r.is_deleted", "=", (int64_t)0);
            qb->where("u.state", "=", (int64_t)1);
            qb->executeCount(stats.active_members, conn);
        }
    }

    return stats;
}
