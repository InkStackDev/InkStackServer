#include "SystemSettingsManager.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>

#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

void SystemSettingsManager::add(DB::SystemSettingsInfo::ptr info)
{
    std::unique_lock<std::mutex> lock(mutex_);
    data_ = info;
}

DB::SystemSettingsInfo::ptr SystemSettingsManager::get()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (data_) {
            return data_;
        }
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = DB::SystemSettingsInfoDao::Query(1, db);
    if (!info) {
        // 创建默认行
        info.reset(new DB::SystemSettingsInfo);
        info->setId(1);
        if (DB::SystemSettingsInfoDao::Insert(info, db)) {
            ERROR(logger) << "SystemSettingsManager insert default row fail";
            return info;
        }
    }
    std::unique_lock<std::mutex> lock(mutex_);
    data_ = info;
    return info;
}

DB::SystemSettingsInfo::ptr SystemSettingsManager::clone()
{
    auto current = get();
    if (!current) {
        return nullptr;
    }
    return std::make_shared<DB::SystemSettingsInfo>(*current);
}

bool SystemSettingsManager::update(DB::SystemSettingsInfo::ptr info)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    info->setId(1);
    if (DB::SystemSettingsInfoDao::InsertOrUpdate(info, db)) {
        ERROR(logger) << "SystemSettingsManager update fail";
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    data_ = info;
    return true;
}
