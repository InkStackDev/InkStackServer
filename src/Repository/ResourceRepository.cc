#include "ResourceRepository.hpp"

#include <memory>

#include <Chen/DB/Redis.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Cache/CacheUtil.hpp"

struct ResourceRepositoryCodec {
    static DB::ResourceInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::ResourceInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setName(CacheJsonString(v, "name"));
        info->setPath(CacheJsonString(v, "path"));
        info->setType(CacheJsonInt32(v, "type"));
        info->setSize(CacheJsonInt64(v, "size"));
        info->setHash(CacheJsonString(v, "hash"));
        info->setOwnerId(CacheJsonInt64(v, "owner_id"));
        info->setBizType(CacheJsonString(v, "biz_type"));
        info->setBizId(CacheJsonInt64(v, "biz_id"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::ResourceInfo, ResourceRepositoryCodec> g_resource_cache{{"res", 300}};

static std::string PathKey(const std::string& path)
{
    return "res:path:" + path;
}

DB::ResourceInfo::ptr ResourceRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_resource_cache.Get(id, [id, conn]() {
        return DB::ResourceInfoDao::Query(id, conn);
    });
}

void ResourceRepository::ListByBizUid(std::vector<DB::ResourceInfo::ptr>& results,
    const std::string& biz_type,
    int64_t biz_id,
    int64_t uid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::ResourceInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->where("biz_type", "=", biz_type);
    qb->where("biz_id", "=", biz_id);
    qb->where("owner_id", "=", uid);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    DB::ResourceInfoDao::QueryByBuilder(results, qb, conn);
}

DB::ResourceInfo::ptr ResourceRepository::GetByBizUidName(const std::string& biz_type,
    int64_t biz_id,
    int64_t uid,
    const std::string& filename,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    auto qb = DB::ResourceInfoDao::newQuery();
    if (!qb) {
        return nullptr;
    }
    qb->where("biz_type", "=", biz_type);
    qb->where("biz_id", "=", biz_id);
    qb->where("owner_id", "=", uid);
    qb->where("name", "=", filename);
    qb->limit(1);
    std::vector<DB::ResourceInfo::ptr> results;
    if (DB::ResourceInfoDao::QueryByBuilder(results, qb, conn) || results.empty()) {
        return nullptr;
    }
    Put(results[0]);
    return results[0];
}

DB::ResourceInfo::ptr ResourceRepository::GetByPath(const std::string& path, chen::IDB::ptr conn)
{
    if (!conn || path.empty()) {
        return nullptr;
    }
    const std::string key = PathKey(path);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = Get(cached_id, conn);
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto qb = DB::ResourceInfoDao::newQuery();
    if (!qb) {
        return nullptr;
    }
    qb->where("path", "=", path);
    qb->limit(1);
    std::vector<DB::ResourceInfo::ptr> results;
    if (DB::ResourceInfoDao::QueryByBuilder(results, qb, conn) || results.empty()) {
        return nullptr;
    }
    Put(results[0]);
    CacheIdMapping(key, results[0]->getId());
    return results[0];
}

void ResourceRepository::Put(const DB::ResourceInfo::ptr& info)
{
    g_resource_cache.Put(info);
}

void ResourceRepository::Evict(int64_t id)
{
    g_resource_cache.Evict(id);
}
