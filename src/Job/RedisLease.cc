#include "RedisLease.hpp"

#include <unistd.h>

#include <utility>

#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/RandomUtil.hpp>

#include "../Config/AppConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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
        // Redis 不可用时不阻塞任务，避免后台任务永久停摆
        WARN(logger) << "acquire lease failed, redis unavailable: " << name_;
        return true;
    }
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
