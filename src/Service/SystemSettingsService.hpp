/**
 * @file SystemSettingsService.hpp
 * @brief 系统设置用例：DB 加载、跨 worker 版本校验、更新
 * @author Christins
 * @date 2026-11-28
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/SystemSettingsInfo.hpp"

#include <cstdint>
#include <mutex>

#include <Chen/Util/Singleton.hpp>

/**
 * @brief 系统设置用例
 * @details 单行 upsert 不开显式事务；快照写入 SystemSettingsStore，供只读路径使用。
 */
class SystemSettingsService {
public:
    /**
     * @brief 获取当前设置；必要时从 DB 加载并校验远端版本
     */
    DB::SystemSettingsInfo::ptr get();

    /**
     * @brief 复制当前设置，供写路径修改
     */
    DB::SystemSettingsInfo::ptr clone();

    /**
     * @brief 更新设置并递增跨 worker 版本号
     */
    bool update(DB::SystemSettingsInfo::ptr info);

    /**
     * @brief 按 1s 间隔刷新快照（供 onTick 调用）
     */
    void refreshIfStale();

private:
    /// 读取跨 worker 版本号（Redis）
    static int64_t ReadRemoteVersion();

    /// 设置更新后递增跨 worker 版本号
    static int64_t BumpRemoteVersion();

    /// 从 DB 加载并写入快照
    DB::SystemSettingsInfo::ptr loadFromDb(int64_t remote_version);

    /// 串行化慢路径的 DB/Redis 加载，避免同一 worker 内并发覆盖快照
    std::mutex mutex_;
};

typedef chen::Singleton<SystemSettingsService> SystemSettingsServiceRegistry;
