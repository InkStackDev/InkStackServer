/**
 * @file JobRunner.hpp
 * @brief 后台任务注册与执行（定时器 + Redis 租约）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

/**
 * @brief 后台任务执行器
 * @details 每个任务注册后由 IOManager 定时触发；触发时先抢 Redis 租约，
 *          同一时刻只有一个 worker 执行，避免多 worker / 多实例重复跑。
 *          任务必须幂等、可重入，且不会长期阻塞事件循环。
 */
class JobRunner {
public:
    struct Job {
        /// 任务名，同时作为租约 key 和日志标识
        std::string name;
        /// 触发间隔（毫秒）
        uint64_t interval_ms = 0;
        /// 租约有效期（秒），必须大于任务最长执行时间
        int lease_ttl_sec = 60;
        /// 启动时是否立即执行一次
        bool run_immediately = false;
        /// 任务体
        std::function<void()> run;
    };

    /**
     * @brief 注册任务（必须在 StartAll 之前调用）
     */
    static void Register(Job job);

    /**
     * @brief 启动全部已注册任务
     */
    static void StartAll();

    /**
     * @brief 停止并清空全部任务
     */
    static void StopAll();

    /// 任务运行态（实现细节，供文件级存储使用）
    struct State;

    /**
     * @brief 执行单个任务（定时器回调入口）
     */
    static void Run(const std::shared_ptr<State>& state);
};