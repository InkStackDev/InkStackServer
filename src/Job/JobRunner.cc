#include "JobRunner.hpp"

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Timer/Timer.hpp>

#include "RedisLease.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

struct JobRunner::State {
    Job job;
    std::atomic<bool> running{false};
    chen::Timer::ptr timer;
};

namespace {

std::mutex g_job_mutex;
std::vector<std::shared_ptr<JobRunner::State>> g_jobs;
bool g_job_started = false;

} // namespace

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
            state->running.store(false);
            return;
        }
    }

    try {
        state->job.run();
    } catch (const std::exception& e) {
        ERROR(logger) << "job failed: " << state->job.name << ", what=" << e.what();
    } catch (...) {
        ERROR(logger) << "job failed: " << state->job.name << ", unknown error";
    }
    state->running.store(false);
}
