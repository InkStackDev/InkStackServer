#include "CacheUtil.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>
#include <Chen/Util/Util.hpp>

#include "../Config/AppConfig.hpp"

int64_t ExecuteCountCached(chen::QueryBuilder::ptr qb, chen::IDB::ptr conn, const std::string& cache_key, int ttl_sec)
{
    std::string redisKey = "cache:count:" + cache_key;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", redisKey.c_str());
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    chen::RedisUtil::Cmd(GetRedisPoolName(), "setex %s %d %lld", redisKey.c_str(), ttl_sec, total);
    return total;
}

void CacheListResult(const std::string& cache_key, const std::vector<int64_t>& ids, int ttl_sec)
{
    if (ids.empty()) {
        return;
    }
    std::stringstream ss;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) {
            ss << ",";
        }
        ss << ids[i];
    }
    std::string redisKey = "cache:list:" + cache_key;
    chen::RedisUtil::Cmd(GetRedisPoolName(), "setex %s %d %s", redisKey.c_str(), ttl_sec, ss.str().c_str());
}

bool GetCachedListResult(const std::string& cache_key, std::vector<int64_t>& ids)
{
    std::string redisKey = "cache:list:" + cache_key;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", redisKey.c_str());
    if (!rpy || !rpy->str) {
        return false;
    }
    std::string data(rpy->str);
    size_t pos = 0;
    while (pos < data.size()) {
        size_t end = data.find(',', pos);
        if (end == std::string::npos) {
            end = data.size();
        }
        ids.push_back(chen::TypeUtil::Atoi(data.substr(pos, end - pos)));
        pos = end + 1;
    }
    return true;
}

void CacheStringResult(const std::string& cache_key, const std::string& value, int ttl_sec)
{
    std::string redisKey = "cache:str:" + cache_key;
    chen::RedisUtil::Cmd(GetRedisPoolName(), "setex %s %d %s", redisKey.c_str(), ttl_sec, value.c_str());
}

bool GetCachedStringResult(const std::string& cache_key, std::string& value)
{
    std::string redisKey = "cache:str:" + cache_key;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", redisKey.c_str());
    if (!rpy || !rpy->str) {
        return false;
    }
    value = rpy->str;
    return true;
}

void CacheIdMapping(const std::string& cache_key, int64_t id, int ttl_sec)
{
    std::string redisKey = "cache:map:" + cache_key;
    chen::RedisUtil::Cmd(GetRedisPoolName(), "setex %s %d %lld", redisKey.c_str(), ttl_sec, id);
}

int64_t GetCachedIdMapping(const std::string& cache_key)
{
    std::string redisKey = "cache:map:" + cache_key;
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", redisKey.c_str());
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    return 0;
}

bool IsObjectCacheEnabled()
{
    return GetObjCacheEnabled();
}

std::string RedisCacheGet(const std::string& key)
{
    if (!IsObjectCacheEnabled()) {
        return std::string();
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", key.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING || !rpy->str) {
        return std::string();
    }
    return std::string(rpy->str, rpy->len);
}

bool RedisCacheSetex(const std::string& key, const std::string& value, int ttl_sec)
{
    if (!IsObjectCacheEnabled()) {
        return false;
    }
    chen::ReplyPtr rpy;
    if (ttl_sec > 0) {
        rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "setex %s %d %s", key.c_str(), ttl_sec, value.c_str());
    } else {
        rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "set %s %s", key.c_str(), value.c_str());
    }
    return rpy && rpy->type == REDIS_REPLY_STATUS;
}

bool RedisCacheDel(const std::string& key)
{
    if (!IsObjectCacheEnabled()) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", key.c_str());
    return rpy && rpy->type == REDIS_REPLY_INTEGER && rpy->integer > 0;
}

bool RedisCacheExists(const std::string& key)
{
    if (!IsObjectCacheEnabled()) {
        return false;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "exists %s", key.c_str());
    return rpy && rpy->type == REDIS_REPLY_INTEGER && rpy->integer > 0;
}

int64_t CacheJsonInt64(const Json::Value& v, const char* key)
{
    const Json::Value& field = v[key];
    if (field.isString()) {
        return chen::TypeUtil::Atoi(field.asString());
    }
    if (field.isBool()) {
        return field.asBool() ? 1 : 0;
    }
    if (field.isInt64() || field.isInt() || field.isUInt() || field.isUInt64()) {
        return field.asInt64();
    }
    if (field.isDouble()) {
        return static_cast<int64_t>(field.asDouble());
    }
    return 0;
}

int32_t CacheJsonInt32(const Json::Value& v, const char* key)
{
    return static_cast<int32_t>(CacheJsonInt64(v, key));
}

std::string CacheJsonString(const Json::Value& v, const char* key)
{
    const Json::Value& field = v[key];
    if (field.isString()) {
        return field.asString();
    }
    if (field.isBool()) {
        return field.asBool() ? "1" : "0";
    }
    if (field.isInt64() || field.isInt() || field.isUInt() || field.isUInt64()) {
        return std::to_string(field.asInt64());
    }
    return std::string();
}

int64_t CacheJsonTime(const Json::Value& v, const char* key)
{
    const Json::Value& field = v[key];
    if (field.isString()) {
        return chen::TimeUtil::Str2Time(field.asString());
    }
    return CacheJsonInt64(v, key);
}
