/**
 * @file SystemSettingsStore.hpp
 * @brief 系统设置进程内快照（只读状态，不访问 DB/Redis）
 * @author Christins
 * @date 2026-11-28
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/SystemSettingsInfo.hpp"

#include <cstdint>
#include <memory>
#include <mutex>

#include <Chen/Util/Singleton.hpp>

/**
 * @brief 系统设置快照容器
 * @details 由 SystemSettingsService 负责加载/刷新，本类只做线程安全的读写。
 */
class SystemSettingsStore {
public:
    /**
     * @brief 获取当前快照（只读）
     */
    DB::SystemSettingsInfo::ptr get() const;

    /**
     * @brief 复制当前快照，供写路径修改
     */
    DB::SystemSettingsInfo::ptr clone() const;

    /**
     * @brief 整体替换快照
     */
    void store(DB::SystemSettingsInfo::ptr info, int64_t version);

    /**
     * @brief 本地快照对应的远端版本号
     */
    int64_t version() const;

    /**
     * @brief 上次校验远端版本的时间（毫秒）
     */
    uint64_t lastCheckMs() const;

    /**
     * @brief 记录一次远端版本校验时间
     */
    void markChecked(uint64_t now_ms);

private:
    mutable std::mutex mutex_;
    DB::SystemSettingsInfo::ptr data_;
    int64_t version_ = -1;
    uint64_t last_check_ms_ = 0;
};

typedef chen::Singleton<SystemSettingsStore> SystemSettingsStoreRegistry;
