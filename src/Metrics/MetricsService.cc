#include "MetricsService.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Chen/DB/Redis.hpp>
#include <Chen/Fiber/Fiber.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include <unistd.h>

#include "../Config/AppConfig.hpp"
#include "../Message/MessageRouter.hpp"
#include "../Manager/WSConnectionManager.hpp"
#include "../Util/Util.hpp"

/**
 * @brief 路由指标运行态
 * @details 由 s_routes 持有，生命周期与进程一致，GaugeFunc 回调持有其裸指针
 */
struct RouteState {
    RouteMetrics metrics;
    uint64_t start_ms = 0;
    uint64_t sampled_at_ms = 0;
    uint64_t sampled_requests = 0;
    std::atomic<int64_t> recent_qps_milli{0};
};

std::mutex s_mutex;
std::unordered_map<std::string, std::unique_ptr<RouteState>> s_routes;
std::vector<chen::http::HttpServer::ptr> s_http_servers;
std::vector<chen::http::WSServer::ptr> s_ws_servers;
uint64_t s_start_ms = 0;
bool s_inited = false;

void Init(
    const std::string& name,
    const std::string& version,
    const std::vector<chen::http::HttpServer::ptr>& http_servers,
    const std::vector<chen::http::WSServer::ptr>& ws_servers)
{
    std::lock_guard lock(s_mutex);
    if (s_inited) {
        return;
    }
    s_inited = true;
    s_start_ms = chen::TimeUtil::GetCurrentMs();
    s_http_servers = http_servers;
    s_ws_servers = ws_servers;

    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return;
    }

    chen::metrics::Labels labels = {{"name", name}, {"version", version}, {"pid", std::to_string(getpid())}};

    auto info = registry->getGauge("blog_info", "Blog service information", labels);
    if (info) {
        info->set(1);
    }

    registry->getGaugeFunc("blog_process_uptime_seconds", "Blog process uptime in seconds", {}, []() -> int64_t {
        return static_cast<int64_t>((chen::TimeUtil::GetCurrentMs() - s_start_ms) / 1000);
    });

    registry->getGaugeFunc("blog_process_resident_memory_kb", "Blog process resident memory in KB", {}, []() -> int64_t {
        return GetProcessMemoryInfo().rss_kb;
    });

    registry->getGaugeFunc("blog_process_virtual_memory_kb", "Blog process virtual memory in KB", {}, []() -> int64_t {
        return GetProcessMemoryInfo().vm_kb;
    });

    registry->getGaugeFunc("blog_process_threads", "Blog process thread count", {}, []() -> int64_t {
        return GetProcessMemoryInfo().threads;
    });

    registry->getGaugeFunc("blog_system_memory_total_kb", "System total memory in KB", {}, []() -> int64_t {
        return GetSystemMemoryInfo().total_kb;
    });

    registry->getGaugeFunc("blog_system_memory_available_kb", "System available memory in KB", {}, []() -> int64_t {
        return GetSystemMemoryInfo().available_kb;
    });

    registry->getGaugeFunc("blog_fibers", "Current fiber count", {}, []() -> int64_t {
        return chen::Fiber::TotalFibers();
    });

    registry->getGaugeFunc("blog_http_active_connections", "Active HTTP connections", {}, []() -> int64_t {
        int64_t total = 0;
        for (const auto& server : s_http_servers) {
            if (server) {
                total += server->getActiveClientCount();
            }
        }
        return total;
    });

    registry->getGaugeFunc("blog_ws_active_connections", "Active WebSocket connections", {}, []() -> int64_t {
        int64_t total = 0;
        for (const auto& server : s_ws_servers) {
            if (server) {
                total += server->getActiveClientCount();
            }
        }
        return total;
    });

    registry->getGaugeFunc("blog_online_users", "Online users (deduplicated)", {}, []() -> int64_t {
        return MessageRouter::Get() ? MessageRouter::Get()->OnlineUserCount() : 0;
    });

    registry->getGaugeFunc("blog_ws_online_devices", "Online WebSocket devices", {}, []() -> int64_t {
        auto* manager = WSMgr::GetInstance();
        if (!manager) {
            return 0;
        }
        return manager->connectionCount();
    });
}

RouteMetrics GetRouteMetrics(const std::string& route)
{
    if (route.empty()) {
        return {};
    }

    std::lock_guard lock(s_mutex);
    auto it = s_routes.find(route);
    if (it != s_routes.end()) {
        return it->second->metrics;
    }

    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return {};
    }

    const chen::metrics::Labels labels = {{"route", route}};

    auto state = std::make_unique<RouteState>();
    state->start_ms = chen::TimeUtil::GetCurrentMs();
    state->sampled_at_ms = state->start_ms;
    state->metrics.requests = registry->getCounter("blog_http_requests_total", "Blog HTTP requests total", labels);
    state->metrics.errors = registry->getCounter("blog_http_errors_total", "Blog HTTP error responses total", labels);
    state->metrics.duration = registry->getHistogram("blog_http_request_duration_seconds", "Blog HTTP request duration in seconds", labels);

    RouteState* raw = state.get();
    registry->getGaugeFunc("blog_http_route_avg_qps_milli", "Blog HTTP route average QPS since start multiplied by 1000", labels, [raw]() -> int64_t {
        if (!raw->metrics.requests) {
            return 0;
        }
        const uint64_t now = chen::TimeUtil::GetCurrentMs();
        const uint64_t elapsed_ms = now > raw->start_ms ? now - raw->start_ms : 0;
        if (!elapsed_ms) {
            return 0;
        }
        const double qps = static_cast<double>(raw->metrics.requests->value()) * 1000.0 / static_cast<double>(elapsed_ms);
        return static_cast<int64_t>(qps * 1000.0);
    });

    registry->getGaugeFunc(
        "blog_http_route_recent_qps_milli", "Blog HTTP route QPS in the latest sampling window multiplied by 1000", labels, [raw]() -> int64_t {
            return raw->recent_qps_milli.load(std::memory_order_relaxed);
        });

    const RouteMetrics result = state->metrics;
    s_routes.emplace(route, std::move(state));

    return result;
}

void RecordRequest(const RouteMetrics& route_metrics, uint64_t used_us, int32_t err)
{
    if (route_metrics.requests) {
        route_metrics.requests->inc();
    }
    if (err != 0 && route_metrics.errors) {
        route_metrics.errors->inc();
    }
    if (route_metrics.duration) {
        route_metrics.duration->observe(static_cast<double>(used_us) / 1000000.0);
    }
}

void SampleRates()
{
    const uint64_t now = chen::TimeUtil::GetCurrentMs();

    std::lock_guard lock(s_mutex);
    for (auto& item : s_routes) {
        auto& state = item.second;
        if (!state->metrics.requests) {
            continue;
        }
        const uint64_t total = state->metrics.requests->value();
        const uint64_t elapsed_ms = now > state->sampled_at_ms ? now - state->sampled_at_ms : 0;
        if (elapsed_ms > 0) {
            const uint64_t delta = total >= state->sampled_requests ? total - state->sampled_requests : total;
            const double qps = static_cast<double>(delta) * 1000.0 / static_cast<double>(elapsed_ms);
            state->recent_qps_milli.store(qps * 1000.0, std::memory_order_relaxed);
        }
        state->sampled_requests = total;
        state->sampled_at_ms = now;
    }
}


namespace {

/// 快照 TTL：实例异常退出后最多 30s 从聚合里消失
constexpr int kSnapshotTtlSec = 30;
const char* kSnapshotIndexKey = "metrics:instances";
const char* kSnapshotPrefix = "metrics:inst:";

std::string SnapshotKey()
{
    return std::string(kSnapshotPrefix) + std::to_string(GetServerInstanceId()) + ":" + std::to_string(getpid());
}

std::string LabelsKey(const Json::Value& labels)
{
    std::string key;
    if (!labels.isObject()) {
        return key;
    }
    for (Json::ValueConstIterator it = labels.begin(); it != labels.end(); ++it) {
        key += it.name();
        key += '=';
        key += it->asString();
        key += '\x1f';
    }
    return key;
}

enum class MergeMode {
    Sum,
    Max,
    Min,
    First
};

MergeMode ModeFor(const std::string& name)
{
    if (name == "blog_info") {
        return MergeMode::First;
    }
    if (name == "blog_online_users" || name.rfind("blog_system_memory_", 0) == 0) {
        return MergeMode::Max;
    }
    if (name == "blog_process_uptime_seconds") {
        return MergeMode::Min;
    }
    return MergeMode::Sum;
}

void MergeItem(std::map<std::string, Json::Value>& merged, const Json::Value& item)
{
    const std::string name = item["name"].asString();
    std::string key = name;
    key += '\x1f';
    key += LabelsKey(item["labels"]);

    auto it = merged.find(key);
    if (it == merged.end()) {
        merged.emplace(key, item);
        return;
    }

    MergeMode mode = ModeFor(name);
    if (mode == MergeMode::First) {
        return;
    }

    Json::Value& dst = it->second;

    if (dst.isMember("buckets") && dst["buckets"].isArray()) {
        for (const auto& src_bucket : item["buckets"]) {
            bool found = false;
            for (auto& bucket : dst["buckets"]) {
                if (bucket.isMember("le") && src_bucket.isMember("le")
                    && bucket["le"].asDouble() == src_bucket["le"].asDouble()) {
                    bucket["cumulative"] = Json::Value::UInt64(
                        bucket["cumulative"].asUInt64() + src_bucket["cumulative"].asUInt64());
                    found = true;
                    break;
                }
            }
            if (!found) {
                dst["buckets"].append(src_bucket);
            }
        }
        const Json::UInt64 count = dst["count"].asUInt64() + item["count"].asUInt64();
        dst["count"] = Json::Value::UInt64(count);
        dst["sum"] = dst["sum"].asDouble() + item["sum"].asDouble();
        return;
    }

    const int64_t lhs = dst.isMember("value") ? dst["value"].asInt64() : 0;
    const int64_t rhs = item.isMember("value") ? item["value"].asInt64() : 0;
    int64_t value = lhs + rhs;
    if (mode == MergeMode::Max) {
        value = std::max(lhs, rhs);
    } else if (mode == MergeMode::Min) {
        value = std::min(lhs, rhs);
    }
    dst["value"] = Json::Value::Int64(value);
}

} // namespace

void PublishSnapshot()
{
    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return;
    }

    const std::string key = SnapshotKey();
    const std::string ttl = std::to_string(kSnapshotTtlSec);
    const std::string now = std::to_string(chen::TimeUtil::GetCurrentSec());
    const std::string cleanup_before = std::to_string(chen::TimeUtil::GetCurrentSec() - kSnapshotTtlSec * 2);

    using RedisArgs = std::vector<std::string>;
    chen::RedisUtil::Cmd(GetRedisPoolName(), RedisArgs{"setex", key, ttl, registry->RenderJson()});
    chen::RedisUtil::Cmd(GetRedisPoolName(), RedisArgs{"zadd", kSnapshotIndexKey, now, key});
    chen::RedisUtil::Cmd(
        GetRedisPoolName(), RedisArgs{"zremrangebyscore", kSnapshotIndexKey, "-inf", cleanup_before});
}

bool CollectClusterMetrics(Json::Value& out)
{
    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return false;
    }

    struct Snapshot {
        std::string key;
        Json::Value value;
    };
    std::vector<Snapshot> snapshots;

    Json::Value own;
    if (!chen::JsonUtil::FromString(own, registry->RenderJson())) {
        return false;
    }
    snapshots.push_back({SnapshotKey(), std::move(own)});

    const std::string min_score = std::to_string(chen::TimeUtil::GetCurrentSec() - kSnapshotTtlSec);
    using RedisArgs = std::vector<std::string>;
    auto index = chen::RedisUtil::Cmd(
        GetRedisPoolName(), RedisArgs{"zrangebyscore", kSnapshotIndexKey, min_score, "+inf"});
    if (index && index->type == REDIS_REPLY_ARRAY) {
        const std::string own_key = SnapshotKey();
        for (size_t i = 0; i < index->elements; ++i) {
            const auto* element = index->element[i];
            if (!element || !element->str) {
                continue;
            }
            std::string key(element->str, element->len);
            if (key == own_key) {
                continue;
            }
            auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), RedisArgs{"get", key});
            if (!rpy || rpy->type != REDIS_REPLY_STRING) {
                continue;
            }
            Json::Value value;
            if (!chen::JsonUtil::FromString(value, std::string(rpy->str, rpy->len))) {
                continue;
            }
            snapshots.push_back({std::move(key), std::move(value)});
        }
    }

    // 按快照 key 排序，保证 blog_info 等 pick 类指标的选择是确定性的
    std::sort(snapshots.begin(), snapshots.end(), [](const Snapshot& lhs, const Snapshot& rhs) {
        return lhs.key < rhs.key;
    });

    std::map<std::string, Json::Value> merged;
    for (const auto& snapshot : snapshots) {
        if (!snapshot.value.isMember("metrics") || !snapshot.value["metrics"].isArray()) {
            continue;
        }
        for (const auto& item : snapshot.value["metrics"]) {
            MergeItem(merged, item);
        }
    }

    Json::Value list(Json::arrayValue);
    for (auto& entry : merged) {
        list.append(std::move(entry.second));
    }
    out = Json::Value(Json::objectValue);
    out["metrics"] = std::move(list);
    out["instanceCount"] = static_cast<Json::UInt64>(snapshots.size());
    return true;
}
