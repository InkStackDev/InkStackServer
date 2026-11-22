#include "RedisLease.hpp"

#include <unistd.h>

#include <cstdint>
#include <mutex>
#include <utility>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/RandomUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Config/AppConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

/// 租约失败日志节流：Redis 故障时每 60s 只打一条，避免每个任务每轮刷屏
constexpr int64_t kWarnIntervalSec = 60;

std::mutex g_warn_mutex;
int64_t g_last_warn_sec = 0;
/// 是否处于“Redis 不可用”状态，用于恢复时只打一条 INFO
bool g_unavailable = false;

void NoteUnavailable(const std::string& name)
{
    int64_t now = chen::TimeUtil::GetCurrentSec();
    std::lock_guard lock(g_warn_mutex);
    g_unavailable = true;
    if (g_last_warn_sec != 0 && now - g_last_warn_sec < kWarnIntervalSec) {
        return;
    }
    g_last_warn_sec = now;
    WARN(logger) << "acquire lease failed, redis unavailable: " << name << " (skip this round; suppressed for "
                 << kWarnIntervalSec << "s)";
}

void NoteAvailable()
{
    if (!g_unavailable) {
        return;
    }
    std::lock_guard lock(g_warn_mutex);
    if (g_unavailable) {
        g_unavailable = false;
        g_last_warn_sec = 0;
        INFO(logger) << "redis lease available again";
    }
}

} // namespace

RedisLease::RedisLease(std::string name, int ttl_sec)
    : name_(std::move(name))
    , key_("lock:" + name_)
    , ttl_sec_(ttl_sec > 0 ? ttl_sec : 60)
{
    token_ = std::to_string(::getpid()) + ":" + chen::RandomUtil::RandString(16);
}

RedisLease::~RedisLease()
{
    Release();
}

bool RedisLease::TryAcquire()
{
    if (held_) {
        return true;
    }
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "set %s %s nx ex %d", key_.c_str(), token_.c_str(), ttl_sec_);
    if (!rpy) {
        // 多实例部署下必须 fail-closed：拿不到租约说明无法确认互斥，跳过本轮，
        // 否则每个实例都会执行同一批定时任务（重复发布、重复清理、重复刷盘）。
        NoteUnavailable(name_);
        return false;
    }
    NoteAvailable();
    held_ = rpy->type == REDIS_REPLY_STATUS;
    return held_;
}

void RedisLease::Release()
{
    if (!held_) {
        return;
    }
    static const char* kReleaseScript = "if redis.call('get', KEYS[1]) == ARGV[1] then "
                                        "return redis.call('del', KEYS[1]) end return 0";
    chen::RedisUtil::Cmd(GetRedisPoolName(), "eval %s 1 %s %s", kReleaseScript, key_.c_str(), token_.c_str());
    held_ = false;
}
