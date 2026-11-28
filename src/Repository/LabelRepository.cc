#include "LabelRepository.hpp"

#include <memory>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

struct LabelRepositoryCodec {
    static DB::LabelInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::LabelInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setName(CacheJsonString(v, "name"));
        info->setColor(CacheJsonString(v, "color"));
        info->setDescription(CacheJsonString(v, "description"));
        info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::LabelInfo, LabelRepositoryCodec> g_label_cache{{"lbl", 300}};

static std::string UserNameKey(int64_t user_id, const std::string& name)
{
    return "lbl:uid_name:" + std::to_string(user_id) + ":" + name;
}

DB::LabelInfo::ptr LabelRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_label_cache.Get(id, [id, conn]() {
        return DB::LabelInfoDao::Query(id, conn);
    });
}

DB::LabelInfo::ptr LabelRepository::GetByUserIdName(int64_t user_id, const std::string& name, chen::IDB::ptr conn)
{
    if (!conn) {
        return nullptr;
    }
    const std::string key = UserNameKey(user_id, name);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto cached = Get(cached_id, conn);
        if (cached && cached->getUserId() == user_id && cached->getName() == name) {
            return cached;
        }
    }

    auto info = DB::LabelInfoDao::QueryByUserIdName(user_id, name, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

bool LabelRepository::InsertOrUpdate(DB::LabelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::LabelInfoDao::InsertOrUpdate(info, conn) == 0;
}

bool LabelRepository::Update(DB::LabelInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::LabelInfoDao::Update(info, conn) == 0;
}

bool LabelRepository::ListByUserId(std::vector<DB::LabelInfo::ptr>& infos,
    int64_t user_id,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        return false;
    }
    auto qb = DB::LabelInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("user_id", "=", user_id);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    return DB::LabelInfoDao::QueryByBuilder(infos, qb, conn) == 0;
}

void LabelRepository::Put(const DB::LabelInfo::ptr& info)
{
    g_label_cache.Put(info);
}

void LabelRepository::Evict(int64_t id)
{
    g_label_cache.Evict(id);
}

void LabelRepository::EvictUserIdName(int64_t user_id, const std::string& name)
{
    RedisCacheDel("cache:map:" + UserNameKey(user_id, name));
}

int64_t LabelRepository::CountValid(chen::IDB::ptr conn)
{
    if (!conn) {
        return 0;
    }
    auto qb = DB::LabelInfoDao::newQuery();
    if (!qb) {
        return 0;
    }
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    return total;
}
