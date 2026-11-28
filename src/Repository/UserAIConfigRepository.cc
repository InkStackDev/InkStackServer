#include "UserAIConfigRepository.hpp"

#include <memory>

#include <Chen/DB/Redis.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Manager/CacheUtil.hpp"

struct UserAIConfigRepositoryCodec {
    static DB::UserAiConfigInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::UserAiConfigInfo>();
        info->setId(CacheJsonInt64(v, "id"));
        info->setUserId(CacheJsonInt64(v, "user_id"));
        info->setType(CacheJsonString(v, "type"));
        info->setUrl(CacheJsonString(v, "url"));
        info->setApiKey(CacheJsonString(v, "api_key"));
        info->setModel(CacheJsonString(v, "model"));
        info->setMaxTokens(CacheJsonInt32(v, "max_tokens"));
        info->setCreateTime(CacheJsonTime(v, "create_time"));
        info->setUpdateTime(CacheJsonTime(v, "update_time"));
        info->markClean();
        return info;
    }
};

const EntityCache<DB::UserAiConfigInfo, UserAIConfigRepositoryCodec> g_ai_config_cache{{"ai_cfg", 300}};

static std::string UserKey(int64_t user_id)
{
    return "ai_cfg:uid:" + std::to_string(user_id);
}

DB::UserAiConfigInfo::ptr UserAIConfigRepository::GetByUserId(int64_t user_id, chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return nullptr;
    }
    const std::string key = UserKey(user_id);
    int64_t cached_id = GetCachedIdMapping(key);
    if (cached_id > 0) {
        auto info = g_ai_config_cache.Get(cached_id, [cached_id, conn]() {
            return DB::UserAiConfigInfoDao::Query(cached_id, conn);
        });
        if (info) {
            return info;
        }
        RedisCacheDel("cache:map:" + key);
    }

    auto info = DB::UserAiConfigInfoDao::QueryByUserId(user_id, conn);
    if (info) {
        Put(info);
        CacheIdMapping(key, info->getId());
    }
    return info;
}

DB::UserAiConfigInfo::ptr UserAIConfigRepository::ReloadByUserId(int64_t user_id, chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return nullptr;
    }
    auto info = DB::UserAiConfigInfoDao::QueryByUserId(user_id, conn);
    if (info) {
        Put(info);
    }
    return info;
}

bool UserAIConfigRepository::InsertOrUpdate(DB::UserAiConfigInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserAiConfigInfoDao::InsertOrUpdate(info, conn) == 0;
}

void UserAIConfigRepository::Put(const DB::UserAiConfigInfo::ptr& info)
{
    if (!info) {
        return;
    }
    g_ai_config_cache.Put(info);
    CacheIdMapping(UserKey(info->getUserId()), info->getId());
}

void UserAIConfigRepository::Evict(int64_t id)
{
    g_ai_config_cache.Evict(id);
}
