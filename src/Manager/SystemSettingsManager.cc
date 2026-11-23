#include "SystemSettingsManager.hpp"

#include <ctime>
#include <memory>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Config/AppConfig.hpp"
#include "../Repository/SystemSettingsRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static const char* kSettingsVersionKey = "settings:version";
/// 校验远端版本的最小间隔（毫秒）
static const uint64_t kVersionCheckIntervalMs = 1000;

int64_t SystemSettingsManager::ReadRemoteVersion()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", kSettingsVersionKey);
    if (!rpy || !rpy->str) {
        return 0;
    }
    return chen::TypeUtil::Atoi(rpy->str);
}

int64_t SystemSettingsManager::BumpRemoteVersion()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "incr %s", kSettingsVersionKey);
    if (!rpy || rpy->type != REDIS_REPLY_INTEGER) {
        return 0;
    }
    return rpy->integer;
}

void SystemSettingsManager::add(DB::SystemSettingsInfo::ptr info)
{
    std::unique_lock<std::mutex> lock(mutex_);
    data_ = info;
}

DB::SystemSettingsInfo::ptr SystemSettingsManager::get()
{
    uint64_t now_ms = chen::TimeUtil::GetCurrentMs();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (data_ && now_ms - last_check_ms_ < kVersionCheckIntervalMs) {
            return data_;
        }
    }

    int64_t remote_version = ReadRemoteVersion();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (data_ && remote_version == version_) {
            last_check_ms_ = now_ms;
            return data_;
        }
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = SystemSettingsRepository::Get(1, db);
    if (!info) {
        // 创建默认行
        info.reset(new DB::SystemSettingsInfo);
        info->setId(1);
        if (!SystemSettingsRepository::Insert(info, db)) {
            ERROR(logger) << "SystemSettingsManager insert default row fail";
            return info;
        }
    }
    std::unique_lock<std::mutex> lock(mutex_);
    data_ = info;
    version_ = remote_version;
    last_check_ms_ = now_ms;
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
    // 缓存对象可能来自“默认行插入”路径，created_at 为 0；统一兜底为当前时间
    if (info->getCreatedAt() <= 0) {
        info->setCreatedAt(time(0));
    }
    if (info->getUpdatedAt() <= 0) {
        info->setUpdatedAt(time(0));
    }
    if (!SystemSettingsRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "SystemSettingsManager update fail, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return false;
    }
    int64_t new_version = BumpRemoteVersion();
    std::unique_lock<std::mutex> lock(mutex_);
    data_ = info;
    if (new_version > 0) {
        version_ = new_version;
    }
    last_check_ms_ = chen::TimeUtil::GetCurrentMs();
    return true;
}
