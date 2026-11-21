/**
 * @file UserAIConfigManager.cc
 * @brief 用户AI配置管理器实现
 * @author Christins
 * @date 2026-06-09
 * @copyright Apache 2.0
 */
#include "UserAIConfigManager.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:ai_cfg:";

static std::string AIConfigCacheKey(int64_t user_id)
{
    return std::string(kCachePrefix) + std::to_string(user_id);
}

static data::UserAiConfigInfo::ptr ParseAIConfigCache(const Json::Value& v)
{
    auto info = std::make_shared<data::UserAiConfigInfo>();
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

static data::UserAiConfigInfo::ptr GetAIConfigCache(int64_t user_id)
{
    std::string key = AIConfigCacheKey(user_id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseAIConfigCache(v);
}

static void SetAIConfigCache(const data::UserAiConfigInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(AIConfigCacheKey(info->getUserId()), info->toJsonString(), kCacheTTL);
}

blog::data::UserAiConfigInfo::ptr UserAIConfigManager::getByUserId(int64_t user_id)
{
    auto v = GetAIConfigCache(user_id);
    if (v) {
        return v;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    v = data::UserAiConfigInfoDao::QueryByUserId(user_id, db);
    if (v) {
        SetAIConfigCache(v);
    }
    return v;
}

bool UserAIConfigManager::save(blog::data::UserAiConfigInfo::ptr info)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setUpdateTime(time(0));
    if (!info->getId()) {
        info->setCreateTime(time(0));
    }

    if (data::UserAiConfigInfoDao::InsertOrUpdate(info, db)) {
        ERROR(logger) << "UserAIConfigManager save fail";
        return false;
    }

    // save 之后需要重新查询以获取 DB 生成的 id
    auto saved = data::UserAiConfigInfoDao::QueryByUserId(info->getUserId(), db);
    if (saved) {
        SetAIConfigCache(saved);
    }
    return true;
}
