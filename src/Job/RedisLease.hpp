/**
 * @file RedisLease.hpp
 * @brief Redis 分布式租约（多 worker / 多实例互斥）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

/**
 * @brief 基于 Redis SET NX EX 的短租约
 * @details 用于后台任务、全量重建、外部 API 同步等需要“同一时刻只跑一份”的场景。
 *          Redis 不可用时不阻塞业务：TryAcquire 返回 true，退化成本进程执行。
 */
class RedisLease {
public:
    /**
     * @brief 构造租约
     * @param name 逻辑名称（内部自动拼 `lock:` 前缀）
     * @param ttl_sec 租约有效期，必须大于任务最长执行时间
     */
    RedisLease(std::string name, int ttl_sec);
    ~RedisLease();

    RedisLease(const RedisLease&) = delete;
    RedisLease& operator=(const RedisLease&) = delete;

    /**
     * @brief 尝试获取租约
     * @return true 获取成功（或 Redis 不可用时降级执行），false 已被其它 worker 持有
     */
    bool TryAcquire();

    /**
     * @brief 释放租约（只删除自己持有的 token）
     */
    void Release();

private:
    std::string name_;
    std::string key_;
    std::string token_;
    int ttl_sec_;
    bool held_ = false;
};
