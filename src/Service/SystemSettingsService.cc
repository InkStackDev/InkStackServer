#include "SystemSettingsService.hpp"

#include <memory>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Cache/SystemSettingsStore.hpp"
#include "../Config/AppConfig.hpp"
#include "../Repository/SystemSettingsRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

static const char* kSettingsVersionKey = "settings:version";
/// 校验远端版本的最小间隔（毫秒）
static const uint64_t kVersionCheckIntervalMs = 1000;

int64_t SystemSettingsService::ReadRemoteVersion()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", kSettingsVersionKey);
    if (!rpy || !rpy->str) {
        return 0;
    }
    return chen::TypeUtil::Atoi(rpy->str);
}

int64_t SystemSettingsService::BumpRemoteVersion()
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "incr %s", kSettingsVersionKey);
    if (!rpy || rpy->type != REDIS_REPLY_INTEGER) {
        return 0;
    }
    return rpy->integer;
}

DB::SystemSettingsInfo::ptr SystemSettingsService::loadFromDb(int64_t remote_version)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = SystemSettingsRepository::Get(1, db);
    if (!info) {
        // 创建默认行
        info = std::make_shared<DB::SystemSettingsInfo>();
        info->setId(1);
        if (!SystemSettingsRepository::Insert(info, db)) {
            ERROR(logger) << "SystemSettingsService insert default row fail";
            return info;
        }
    }

    SystemSettingsStoreRegistry::GetInstance()->store(info, remote_version);
    SystemSettingsStoreRegistry::GetInstance()->markChecked(chen::TimeUtil::GetCurrentMs());
    return info;
}

DB::SystemSettingsInfo::ptr SystemSettingsService::get()
{
    auto store = SystemSettingsStoreRegistry::GetInstance();
    {
        const uint64_t now_ms = chen::TimeUtil::GetCurrentMs();
        auto data = store->get();
        if (data && now_ms - store->lastCheckMs() < kVersionCheckIntervalMs) {
            return data;
        }
    }

    std::unique_lock<std::mutex> lock(mutex_);
    const uint64_t now_ms = chen::TimeUtil::GetCurrentMs();
    {
        auto data = store->get();
        if (data && now_ms - store->lastCheckMs() < kVersionCheckIntervalMs) {
            return data;
        }
    }

    const int64_t remote_version = ReadRemoteVersion();
    {
        auto data = store->get();
        if (data && remote_version == store->version()) {
            store->markChecked(now_ms);
            return data;
        }
    }

    return loadFromDb(remote_version);
}

DB::SystemSettingsInfo::ptr SystemSettingsService::clone()
{
    auto current = get();
    if (!current) {
        return nullptr;
    }
    return std::make_shared<DB::SystemSettingsInfo>(*current);
}

bool SystemSettingsService::update(DB::SystemSettingsInfo::ptr info)
{
    if (!info) {
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    const int64_t now_sec = static_cast<int64_t>(chen::TimeUtil::GetCurrentSec());
    info->setId(1);
    // 缓存对象可能来自“默认行插入”路径，created_at 为 0；统一兜底为当前时间
    if (info->getCreatedAt() <= 0) {
        info->setCreatedAt(now_sec);
    }
    if (info->getUpdatedAt() <= 0) {
        info->setUpdatedAt(now_sec);
    }
    if (!SystemSettingsRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "SystemSettingsService update fail, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return false;
    }

    const int64_t new_version = BumpRemoteVersion();
    auto store = SystemSettingsStoreRegistry::GetInstance();
    store->store(info, new_version > 0 ? new_version : store->version());
    store->markChecked(chen::TimeUtil::GetCurrentMs());
    return true;
}

void SystemSettingsService::refreshIfStale()
{
    auto store = SystemSettingsStoreRegistry::GetInstance();
    if (chen::TimeUtil::GetCurrentMs() - store->lastCheckMs() < kVersionCheckIntervalMs) {
        return;
    }
    get();
}
