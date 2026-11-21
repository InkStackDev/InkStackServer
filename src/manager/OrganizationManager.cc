#include "OrganizationManager.hpp"

#include "../util/Util.hpp"
#include "CacheUtil.hpp"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 120;
static const char* kCachePrefix = "cache:obj:org:";

static std::string OrganizationCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static db::OrganizationInfo::ptr ParseOrganizationCache(const Json::Value& v)
{
    auto info = std::make_shared<db::OrganizationInfo>();
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

static data::OrganizationInfo::ptr GetOrganizationCache(int64_t id)
{
    std::string key = OrganizationCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseOrganizationCache(v);
}

static void SetOrganizationCache(const data::OrganizationInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(OrganizationCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::OrganizationInfo::ptr OrganizationManager::ParseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationInfoDao::ParseRow(rt);
}

void OrganizationManager::add(data::OrganizationInfo::ptr info)
{
    SetOrganizationCache(info);
}

data::OrganizationInfo::ptr OrganizationManager::get(int64_t id)
{
    auto v = GetOrganizationCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::OrganizationInfoDao::Query(id, db);
    if (v) {
        SetOrganizationCache(v);
    }
    return v;
}

data::OrganizationInfo::ptr OrganizationManager::getByName(const std::string& name)
{
    int64_t cachedId = GetCachedIdMapping("org:name:" + name);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::OrganizationInfoDao::QueryByName(name, db);
    if (info) {
        SetOrganizationCache(info);
        CacheIdMapping("org:name:" + name, info->getId());
    }
    return info;
}

int64_t OrganizationManager::listByPages(
    std::vector<data::OrganizationInfo::ptr>& orgs,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::OrganizationInfoDao::QueryByBuilderPages(orgs, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        return 0;
    }
    return total;
}

OrganizationManager::OrganizationStats OrganizationManager::getStats()
{
    OrganizationStats stats;

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

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // Query total (non-deleted)
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        int64_t total = 0;
        if (qb->executeCount(total, db) == 0) {
            stats.total = total;
        }
    }

    // Query active
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        qb->where("status", "=", (int64_t)Status::ACTIVE);
        int64_t active = 0;
        if (qb->executeCount(active, db) == 0) {
            stats.active = active;
        }
    }

    // Query inactive
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        qb->where("status", "=", (int64_t)Status::INACTIVE);
        int64_t inactive = 0;
        if (qb->executeCount(inactive, db) == 0) {
            stats.inactive = inactive;
        }
    }

    std::stringstream ss;
    ss << stats.total << "|" << stats.active << "|" << stats.inactive;
    CacheStringResult("org:stats", ss.str());

    return stats;
}
