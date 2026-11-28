#include "SystemSettingsStore.hpp"

#include <memory>
#include <mutex>

DB::SystemSettingsInfo::ptr SystemSettingsStore::get() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return data_;
}

DB::SystemSettingsInfo::ptr SystemSettingsStore::clone() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!data_) {
        return nullptr;
    }
    return std::make_shared<DB::SystemSettingsInfo>(*data_);
}

void SystemSettingsStore::store(DB::SystemSettingsInfo::ptr info, int64_t version)
{
    std::unique_lock<std::mutex> lock(mutex_);
    data_ = std::move(info);
    version_ = version;
}

int64_t SystemSettingsStore::version() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return version_;
}

uint64_t SystemSettingsStore::lastCheckMs() const
{
    std::unique_lock<std::mutex> lock(mutex_);
    return last_check_ms_;
}

void SystemSettingsStore::markChecked(uint64_t now_ms)
{
    std::unique_lock<std::mutex> lock(mutex_);
    last_check_ms_ = now_ms;
}
