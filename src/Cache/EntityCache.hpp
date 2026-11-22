/**
 * @file EntityCache.hpp
 * @brief 实体对象 Redis 缓存（读穿 / 写穿 / 失效）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <json/json.h>

#include <Chen/Util/JsonUtil.hpp>

#include "../Manager/CacheUtil.hpp"

/**
 * @brief 单实体缓存策略
 */
struct CachePolicy {
    /// 缓存区域，最终 key 为 cache:obj:{area}:{id}
    const char* area = "";
    /// 过期时间（秒），必须大于 0
    int ttl_sec = 60;
};

/**
 * @brief 单实体缓存模板
 * @tparam Info  ORM 生成的 Info 类型（需提供 ptr / getId / toJsonString）
 * @tparam Codec 反序列化器（需提供 static Info::ptr Decode(const Json::Value&)）
 * @details 只负责缓存读写，不负责事务；调用方必须在 DB commit 成功后再 Put/Evict。
 */
template <typename Info, typename Codec>
class EntityCache {
public:
    explicit EntityCache(CachePolicy policy)
        : policy_(policy)
    {
    }

    /**
     * @brief 读缓存，未命中时调用 loader 并回填
     */
    template <typename Loader>
    typename Info::ptr Get(int64_t id, Loader&& loader) const
    {
        const std::string key = Key(id);
        std::string data = RedisCacheGet(key);
        if (!data.empty()) {
            Json::Value v;
            if (chen::JsonUtil::FromString(v, data) && v.isObject()) {
                return Codec::Decode(v);
            }
            RedisCacheDel(key);
        }

        auto info = loader();
        if (info) {
            Put(info);
        }
        return info;
    }

    /**
     * @brief 写入缓存（DB commit 成功后调用）
     */
    void Put(const typename Info::ptr& info) const
    {
        if (!info) {
            return;
        }
        RedisCacheSetex(Key(info->getId()), info->toJsonString(), policy_.ttl_sec);
    }

    /**
     * @brief 删除缓存
     */
    void Evict(int64_t id) const { RedisCacheDel(Key(id)); }

private:
    std::string Key(int64_t id) const { return std::string("cache:obj:") + policy_.area + ":" + std::to_string(id); }

    CachePolicy policy_;
};
