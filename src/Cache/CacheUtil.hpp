/**
 * @file CacheUtil.hpp
 * @brief Redis 缓存工具（COUNT 缓存、列表结果缓存）
 * @author Christins
 * @date 2026-06-06
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/DB/QueryBuilder.hpp>

#include <json/json.h>

/// COUNT 缓存 TTL（秒）
static const int kCountCacheTTL = 30;
/// 列表结果缓存 TTL（秒）
static const int kListCacheTTL = 10;
/// Stats 缓存 TTL（秒）
static const int kStatsCacheTTL = 60;
/// ID 映射缓存 TTL（秒）— alternate_key → id
static const int kIdMapCacheTTL = 300;

/**
 * @brief 执行 COUNT 查询，优先从 Redis 缓存读取
 * @param qb QueryBuilder（已配置好 WHERE/JOIN）
 * @param conn 数据库连接
 * @param cache_key 缓存键（不含前缀）
 * @param ttl_sec 缓存 TTL 秒数
 * @return COUNT 结果，失败返回 0
 */
int64_t ExecuteCountCached(
    chen::QueryBuilder::ptr qb,
    chen::IDB::ptr conn,
    const std::string& cache_key,
    int ttl_sec = kCountCacheTTL);

/**
 * @brief 缓存分页列表结果（ID 数组）
 * @param cache_key 缓存键
 * @param ids 文章/评论 ID 列表
 * @param ttl_sec TTL 秒数
 */
void CacheListResult(const std::string& cache_key, const std::vector<int64_t>& ids, int ttl_sec = kListCacheTTL);

/**
 * @brief 读取缓存的分页列表结果
 * @param cache_key 缓存键
 * @param[out] ids 解析后的 ID 列表
 * @return true 表示命中缓存
 */
bool GetCachedListResult(const std::string& cache_key, std::vector<int64_t>& ids);

/**
 * @brief 缓存字符串结果
 * @param cache_key 缓存键
 * @param value 字符串值
 * @param ttl_sec TTL 秒数
 */
void CacheStringResult(const std::string& cache_key, const std::string& value, int ttl_sec = kStatsCacheTTL);

/**
 * @brief 读取缓存的字符串结果
 * @param cache_key 缓存键
 * @param[out] value 缓存的字符串
 * @return true 表示命中缓存
 */
bool GetCachedStringResult(const std::string& cache_key, std::string& value);

/**
 * @brief 缓存 alternate_key → id 映射
 * @param cache_key 缓存键（不含前缀）
 * @param id 主键 ID
 * @param ttl_sec TTL 秒数
 */
void CacheIdMapping(const std::string& cache_key, int64_t id, int ttl_sec = kIdMapCacheTTL);

/**
 * @brief 读取 alternate_key → id 映射
 * @param cache_key 缓存键（不含前缀）
 * @return ID，未命中返回 0
 */
int64_t GetCachedIdMapping(const std::string& cache_key);

/**
 * @brief Redis 对象缓存是否开启（cache.obj.enabled）
 */
bool IsObjectCacheEnabled();

/**
 * @brief 读取 Redis 字符串（各 Manager 对象缓存的基础操作）
 * @param key 完整 key
 * @return 值，未命中/失败/缓存关闭时返回空串
 */
std::string RedisCacheGet(const std::string& key);

/**
 * @brief 写入 Redis 字符串并设置 TTL（各 Manager 对象缓存的基础操作）
 * @param key 完整 key
 * @param value 值
 * @param ttl_sec TTL 秒数，<= 0 表示不设置过期
 * @return true 成功
 */
bool RedisCacheSetex(const std::string& key, const std::string& value, int ttl_sec);

/**
 * @brief 删除 Redis key（各 Manager 对象缓存的基础操作）
 * @return true 删除成功（key 存在）
 */
bool RedisCacheDel(const std::string& key);

/**
 * @brief 判断 Redis key 是否存在（各 Manager 对象缓存的基础操作）
 */
bool RedisCacheExists(const std::string& key);

/**
 * @brief 从缓存 JSON 中读取 int64（兼容字符串/数字）
 */
int64_t CacheJsonInt64(const Json::Value& v, const char* key);

/**
 * @brief 从缓存 JSON 中读取 int32
 */
int32_t CacheJsonInt32(const Json::Value& v, const char* key);

/**
 * @brief 从缓存 JSON 中读取 string
 */
std::string CacheJsonString(const Json::Value& v, const char* key);

/**
 * @brief 从缓存 JSON 中读取时间戳（兼容 "YYYY-MM-DD HH:MM:SS" 与数字）
 */
int64_t CacheJsonTime(const Json::Value& v, const char* key);
