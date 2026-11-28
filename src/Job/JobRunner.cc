#include "JobRunner.hpp"

#include <unistd.h>

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <Chen/DB/Redis.hpp>
#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Timer/Timer.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Config/AppConfig.hpp"
#include "RedisLease.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

struct JobRunner::State {
    Job job;
    std::atomic<bool> running{false};
    chen::Timer::ptr timer;
    int64_t last_skip_log_sec = 0;
};

static std::mutex g_job_mutex;
static std::vector<std::shared_ptr<JobRunner::State>> g_jobs;
static bool g_job_started = false;

void JobRunner::Register(Job job)
{
    if (job.name.empty() || !job.run || job.interval_ms == 0) {
        ERROR(logger) << "invalid job registration: name=" << job.name;
        return;
    }

    auto state = std::make_shared<State>();
    state->job = std::move(job);

    std::lock_guard lock(g_job_mutex);
    if (g_job_started) {
        ERROR(logger) << "job registered after StartAll, ignored: " << state->job.name;
        return;
    }
    for (auto& item : g_jobs) {
        if (item && item->job.name == state->job.name) {
            item->job = std::move(state->job);
            return;
        }
    }
    g_jobs.push_back(std::move(state));
}

void JobRunner::StartAll()
{
    auto iom = chen::IOManager::GetThis();
    if (!iom) {
        ERROR(logger) << "start jobs failed: no IOManager";
        return;
    }

    std::lock_guard lock(g_job_mutex);
    g_job_started = true;
    for (auto& state : g_jobs) {
        if (!state || !state->job.run) {
            continue;
        }
        if (state->timer) {
            state->timer->cancel();
        }
        auto weak = std::weak_ptr<State>(state);
        state->timer = iom->addTimer(
            state->job.interval_ms,
            [weak]() {
                if (auto s = weak.lock()) {
                    JobRunner::Run(s);
                }
            },
            true);

        if (state->job.run_immediately) {
            iom->schedule([weak]() {
                if (auto s = weak.lock()) {
                    JobRunner::Run(s);
                }
            });
        }
    }
    INFO(logger) << "started " << g_jobs.size() << " background jobs";
}

void JobRunner::StopAll()
{
    std::vector<std::shared_ptr<State>> jobs;
    {
        std::lock_guard lock(g_job_mutex);
        jobs.swap(g_jobs);
        g_job_started = false;
    }
    for (auto& state : jobs) {
        if (state && state->timer) {
            state->timer->cancel();
            state->timer.reset();
        }
    }
    INFO(logger) << "stopped " << jobs.size() << " background jobs";
}

void JobRunner::Run(const std::shared_ptr<State>& state)
{
    if (!state || !state->job.run) {
        return;
    }
    if (state->running.exchange(true)) {
        DEBUG(logger) << "job still running, skip: " << state->job.name;
        return;
    }

    std::unique_ptr<RedisLease> lease;
    if (state->job.use_lease) {
        lease = std::make_unique<RedisLease>("job:" + state->job.name, state->job.lease_ttl_sec);
        if (!lease->TryAcquire()) {
            DEBUG(logger) << "job lease busy, skip: " << state->job.name << ", instance=" << GetServerInstanceId();
            state->running.store(false);
            return;
        }
    }

    const auto& job = state->job;
    const int64_t window_sec = job.dedup_interval_ms / 1000;
    if (window_sec > 0) {
        int64_t now = chen::TimeUtil::GetCurrentSec();
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get job:last:%s", job.name.c_str());
        if (rpy && rpy->type == REDIS_REPLY_STRING && rpy->str) {
            int64_t last = chen::TypeUtil::Atoi(std::string(rpy->str, rpy->len));
            if (last > 0 && now - last < window_sec) {
                // 每个实例最多每小时打一条，方便确认“本轮由别的实例执行了”
                if (now - state->last_skip_log_sec >= 3600) {
                    state->last_skip_log_sec = now;
                    INFO(logger) << "job not due, skip: " << job.name << ", instance=" << GetServerInstanceId()
                                 << ", last=" << last << ", next_in=" << (window_sec - (now - last)) << "s";
                }
                state->running.store(false);
                return;
            }
        }
    }

    const bool log_run = window_sec > 0;
    if (log_run) {
        INFO(logger) << "job start: " << job.name << ", instance=" << GetServerInstanceId() << ", pid=" << ::getpid();
    }

    bool ok = true;
    int64_t begin_ms = chen::TimeUtil::GetCurrentMs();
    try {
        job.run();
    } catch (const std::exception& e) {
        ok = false;
        ERROR(logger) << "job failed: " << job.name << ", what=" << e.what();
    } catch (...) {
        ok = false;
        ERROR(logger) << "job failed: " << job.name << ", unknown error";
    }

    if (window_sec > 0 && ok) {
        int64_t finish = chen::TimeUtil::GetCurrentSec();
        int ttl_sec = window_sec * 2;
        chen::RedisUtil::Cmd(GetRedisPoolName(), "set job:last:%s %lld ex %d", job.name.c_str(), finish, ttl_sec);
    }

    if (log_run) {
        int64_t elapsed_ms = chen::TimeUtil::GetCurrentMs() - begin_ms;
        INFO(logger) << "job finished: " << job.name << ", elapsed=" << elapsed_ms << "ms" << (ok ? "" : ", failed");
    }
    state->running.store(false);
}
