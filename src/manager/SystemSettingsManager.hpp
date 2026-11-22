#pragma once

#include "DB/SystemSettingsInfo.hpp"

#include <mutex>

#include <Chen/Util/Singleton.hpp>

class SystemSettingsManager {
public:
    void add(DB::SystemSettingsInfo::ptr info);

    /**
     * @brief 获取当前系统设置
     * @note 返回的是全局共享对象，只能读取，不允许修改字段
     */
    DB::SystemSettingsInfo::ptr get();

    /**
     * @brief 复制当前系统设置，供写路径修改
     * @details 读请求与写请求并发时，直接改共享对象会让读侧拷贝到正在析构/改写的 std::string，
     *          因此写路径必须先 clone 再 update，由 update 整体替换缓存指针。
     */
    DB::SystemSettingsInfo::ptr clone();

    bool update(DB::SystemSettingsInfo::ptr info);

private:
    std::mutex mutex_;
    DB::SystemSettingsInfo::ptr data_;
};

typedef chen::Singleton<SystemSettingsManager> SystemSettingsMgr;
