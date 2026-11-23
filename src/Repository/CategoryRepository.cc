#include "CategoryRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

namespace {

struct CategoryCodec {
    static DB::CategoryInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::CategoryInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setName(CacheJsonString(v, "name"));
        info->setColor(CacheJsonString(v, "color"));
        info->setDescription(CacheJsonString(v, "description"));
        info->setUrl(CacheJsonString(v, "url"));
        info->setIcon(CacheJsonString(v, "icon"));
        info->setParentId(CacheJsonInt64(v, "parent_id"));
        info->setStatus(CacheJsonInt32(v, "status"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::CategoryInfo, CategoryCodec> g_category_cache{{"cat2", 300}};

std::string UserNameKey(int64_t user_id, const std::string& name)
{
    return "cat:uid_name:" + std::to_string(user_id) + ":" + name;
}

} // namespace

DB::CategoryInfo::ptr CategoryRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_category_cache.Get(id, [id, conn]() {
        return DB::CategoryInfoDao::Query(id, conn);
    });
}

DB::CategoryInfo::ptr CategoryRepository::GetByUserIdName(int64_t user_id, const std::string& name, chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = UserNameKey(user_id, name);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto cached = Get(cached_id, conn);
        // 改名后旧映射可能仍在缓存中，校验归属和名称，避免命中脏数据
        if (cached && cached->getUserId() == user_id && cached->getName() == name) {
            return cached;
        }
    }

    auto info = DB::CategoryInfoDao::QueryByUserIdName(user_id, name, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool CategoryRepository::InsertOrUpdate(DB::CategoryInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::CategoryInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool CategoryRepository::Update(DB::CategoryInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::CategoryInfoDao::Update(info, conn) == 0;
}

void CategoryRepository::ListAll(std::vector<DB::CategoryInfo::ptr>& infos, bool is_valid, chen::IDB::ptr conn)
{
    if (!conn) {
        return;
    }
    auto qb = DB::CategoryInfoDao::newQuery();
    if (!qb) {
        return;
    }
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    DB::CategoryInfoDao::QueryByBuilder(infos, qb, conn);
}

bool CategoryRepository::ListByUserId(std::vector<DB::CategoryInfo::ptr>& infos,
    int64_t user_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::CategoryInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("user_id", "=", user_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    return DB::CategoryInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

void CategoryRepository::Put(const DB::CategoryInfo::ptr& info)
{
    g_category_cache.Put(info);
}

void CategoryRepository::Evict(int64_t id)
{
    g_category_cache.Evict(id);
}

void CategoryRepository::EvictUserIdName(int64_t user_id, const std::string& name)
{
    RedisCacheDel("cache:map:" + UserNameKey(user_id, name));
}
