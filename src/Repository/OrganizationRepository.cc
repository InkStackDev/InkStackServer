#include "OrganizationRepository.hpp"

#include <memory>
#include <sstream>

#include <Chen/DB/Redis.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Config/AppConfig.hpp"
#include "../Domain/OrganizationTypes.hpp"
#include "../Manager/CacheUtil.hpp"
#include "../Util/Util.hpp"
#include "DB/OrganizationUserRelInfo.hpp"

namespace {

struct OrganizationCodec {
    static DB::OrganizationInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::OrganizationInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setName(CacheJsonString(v, "name"));
        info->setType(CacheJsonString(v, "type"));
        info->setDescription(CacheJsonString(v, "description"));
        info->setOwnerId(CacheJsonInt64(v, "owner_id"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::OrganizationInfo, OrganizationCodec> g_org_cache{{"org", 120}};

std::string NameKey(const std::string& name)
{
    return "org:name:" + name;
}

constexpr int32_t kStatusInactive = 0;
constexpr int32_t kStatusActive = 1;

} // namespace

DB::OrganizationInfo::ptr OrganizationRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_org_cache.Get(id, [id, conn]() {
        return DB::OrganizationInfoDao::Query(id, conn);
    });
}

DB::OrganizationInfo::ptr OrganizationRepository::GetByName(const std::string& name, chen::IDB::ptr conn)
{
    if (!conn || name.empty()) {
        return nullptr;
    }
    const std::string key = NameKey(name);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::OrganizationInfoDao::QueryByName(name, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool OrganizationRepository::Insert(DB::OrganizationInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationInfoDao::Insert(info, conn) == 0;
}

bool OrganizationRepository::InsertOrUpdate(DB::OrganizationInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool OrganizationRepository::Update(DB::OrganizationInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::OrganizationInfoDao::Update(info, conn) == 0;
}

void OrganizationRepository::Put(const DB::OrganizationInfo::ptr& info)
{
    g_org_cache.Put(info);
}

void OrganizationRepository::Evict(int64_t id)
{
    g_org_cache.Evict(id);
}

void OrganizationRepository::EvictName(const std::string& name)
{
    RedisCacheDel("cache:map:" + NameKey(name));
}

int64_t OrganizationQueryRepository::ListByPages(std::vector<DB::OrganizationInfo::ptr>& orgs,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::OrganizationInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::OrganizationInfoDao::QueryByBuilderPages(orgs, total, qb, (int32_t)offset, (int32_t)limit, conn)) {
        return 0;
    }
    return total;
}

bool OrganizationQueryRepository::ListUserOrgGrants(std::vector<std::pair<int64_t, int32_t>>& grants,
    int64_t user_id,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return false;
    }

    auto qb = DB::OrganizationUserRelInfoDao::newQuery("rel");
    if (!qb) {
        return false;
    }
    qb->select("rel.org_id, rel.role");
    qb->join("INNER", "organization org", "rel.org_id = org.id");
    qb->where("rel.user_id", "=", user_id);
    qb->where("rel.status", "=", (int64_t)OrgMemberStatus::APPROVED);
    qb->where("rel.is_deleted", "=", (int64_t)0);
    qb->where("org.is_deleted", "=", (int64_t)0);
    qb->where("org.status", "=", (int64_t)OrgStatus::ACTIVE);
    return qb->queryPairs<int64_t, int32_t>(grants, conn) == 0;
}

OrganizationQueryRepository::Stats OrganizationQueryRepository::GetStats(chen::IDB::ptr conn)
{
    Stats stats;
    if (!conn) {
        return stats;
    }

    std::string cached;
    if (GetCachedStringResult("org:stats", cached)) {
        std::stringstream ss(cached);
        std::string token;
        auto next = [&]() -> int64_t {
            std::getline(ss, token, '|');
            return chen::TypeUtil::Atoi(token);
        };
        stats.total = next();
        stats.active = next();
        stats.inactive = next();
        return stats;
    }

    {
        auto qb = DB::OrganizationInfoDao::newQuery();
        if (qb) {
            qb->where("is_deleted", "=", (int64_t)0);
            qb->executeCount(stats.total, conn);
        }
    }
    {
        auto qb = DB::OrganizationInfoDao::newQuery();
        if (qb) {
            qb->where("is_deleted", "=", (int64_t)0);
            qb->where("status", "=", (int64_t)kStatusActive);
            qb->executeCount(stats.active, conn);
        }
    }
    {
        auto qb = DB::OrganizationInfoDao::newQuery();
        if (qb) {
            qb->where("is_deleted", "=", (int64_t)0);
            qb->where("status", "=", (int64_t)kStatusInactive);
            qb->executeCount(stats.inactive, conn);
        }
    }

    std::stringstream ss;
    ss << stats.total << "|" << stats.active << "|" << stats.inactive;
    CacheStringResult("org:stats", ss.str());
    return stats;
}

void OrganizationQueryRepository::InvalidateStats()
{
    chen::RedisUtil::Cmd(GetRedisPoolName(), "del cache:str:org:stats");
}
