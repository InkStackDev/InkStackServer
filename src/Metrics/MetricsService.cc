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
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include <unistd.h>

#include "../Config/AppConfig.hpp"
#include "../Message/MessageRouter.hpp"
#include "../Manager/WSConnectionManager.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/**
 * @brief 路由指标注册表：key 为 `route` + `method`
 * @details 指标对象由 registry 持有、生命周期与进程一致，这里只缓存指针，
 *          避免每条请求都走 registry 的加锁查找。
 */
std::mutex s_mutex;
std::unordered_map<std::string, RouteMetrics> s_routes;
std::vector<chen::http::HttpServer::ptr> s_http_servers;
std::vector<chen::http::WSServer::ptr> s_ws_servers;
uint64_t s_start_ms = 0;
bool s_inited = false;
std::atomic<int64_t> s_last_publish_sec{0};
chen::metrics::Counter* s_ws_connect_total = nullptr;
chen::metrics::Counter* s_ws_disconnect_total = nullptr;
std::map<std::string, chen::metrics::Counter*> s_ws_connect_fail;

/// 路由指标 key：路径与方法用不可见分隔符拼接，避免与真实路径冲突
static std::string RouteKey(const std::string& route, const std::string& method)
{
    std::string key = route;
    key += '\x1f';
    key += method;
    return key;
}

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

    registry->getGaugeFunc(
        "blog_metrics_snapshot_age_seconds", "Seconds since last metrics snapshot publish", {}, []() -> int64_t {
            int64_t last = s_last_publish_sec.load(std::memory_order_relaxed);
            if (last <= 0) {
                return -1;
            }
            return static_cast<int64_t>(chen::TimeUtil::GetCurrentSec()) - last;
        });

    s_ws_connect_total = registry->getCounter("blog_ws_connect_total", "WebSocket handshake attempts", {});
    s_ws_disconnect_total = registry->getCounter("blog_ws_disconnect_total", "WebSocket disconnects", {});
    for (const char* reason : {"no_cookie", "not_logged_in", "invalid_session", "internal"}) {
        s_ws_connect_fail[reason] =
            registry->getCounter("blog_ws_connect_fail_total", "WebSocket handshake failures", {{"reason", reason}});
    }
}

RouteMetrics GetRouteMetrics(const std::string& route, const std::string& method)
{
    if (route.empty()) {
        return {};
    }

    const std::string key = RouteKey(route, method);

    std::lock_guard lock(s_mutex);
    auto it = s_routes.find(key);
    if (it != s_routes.end()) {
        return it->second;
    }

    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return {};
    }

    const chen::metrics::Labels labels = {{"route", route}, {"method", method}};

    RouteMetrics metrics;
    metrics.requests = registry->getCounter("blog_http_requests_total", "Blog HTTP requests total", labels);
    metrics.errors = registry->getCounter("blog_http_errors_total", "Blog HTTP error responses total", labels);
    metrics.duration =
        registry->getHistogram("blog_http_request_duration_seconds", "Blog HTTP request duration in seconds", labels);

    s_routes.emplace(key, metrics);
    return metrics;
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

/// 快照 TTL：实例异常退出后最多 60s 从聚合里消失
constexpr int kSnapshotTtlSec = 60;
static const char* kSnapshotIndexKey = "metrics:instances";
static const char* kSnapshotPrefix = "metrics:inst:";

static std::atomic<int64_t> s_last_degraded_warn_sec{0};

static std::string SnapshotKey()
{
    return std::string(kSnapshotPrefix) + std::to_string(GetServerInstanceId()) + ":" + std::to_string(getpid());
}

/**
 * @brief 是否为路由级指标（labels 里带 route）
 */
static bool IsRouteScoped(const Json::Value& item)
{
    return item.isMember("labels") && item["labels"].isObject() && item["labels"].isMember("route");
}

/**
 * @brief 路由级指标的空值裁剪
 * @details 路由在注册时就建好 series，但绝大多数接口在任意时刻都是 0 流量。
 *          直接跳过零值可以把快照体积压到“有流量的路由数”量级；
 *          聚合侧缺失即视作 0，语义不变。
 */
static bool IsEmptyRouteMetric(const Json::Value& item)
{
    if (!IsRouteScoped(item)) {
        return false;
    }
    if (item.isMember("count")) {
        return item["count"].asUInt64() == 0;
    }
    if (item.isMember("value")) {
        return item["value"].asInt64() == 0;
    }
    return false;
}

/**
 * @brief 渲染本进程快照并裁剪零值路由指标
 */
static bool BuildSnapshot(Json::Value& out)
{
    auto* registry = chen::metrics::RegistryMgr::GetInstance();
    if (!registry) {
        return false;
    }
    if (!chen::JsonUtil::FromString(out, registry->RenderJson())) {
        return false;
    }
    if (!out.isMember("metrics") || !out["metrics"].isArray()) {
        return false;
    }

    Json::Value filtered(Json::arrayValue);
    for (const auto& item : out["metrics"]) {
        if (!item.isObject() || IsEmptyRouteMetric(item)) {
            continue;
        }
        filtered.append(item);
    }
    out["metrics"] = std::move(filtered);
    return true;
}

static std::string LabelsKey(const Json::Value& labels)
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

enum class MetricsServiceMergeMode {
    Sum,
    Max,
    Min,
    First
};

static MetricsServiceMergeMode ModeFor(const std::string& name)
{
    if (name == "blog_info") {
        return MetricsServiceMergeMode::First;
    }
    if (name == "blog_online_users" || name.rfind("blog_system_memory_", 0) == 0
        || name == "blog_metrics_snapshot_age_seconds") {
        return MetricsServiceMergeMode::Max;
    }
    if (name == "blog_process_uptime_seconds") {
        return MetricsServiceMergeMode::Min;
    }
    return MetricsServiceMergeMode::Sum;
}

static void MergeItem(std::map<std::string, Json::Value>& merged, const Json::Value& item)
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

    MetricsServiceMergeMode mode = ModeFor(name);
    if (mode == MetricsServiceMergeMode::First) {
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
    if (mode == MetricsServiceMergeMode::Max) {
        value = std::max(lhs, rhs);
    } else if (mode == MetricsServiceMergeMode::Min) {
        value = std::min(lhs, rhs);
    }
    dst["value"] = Json::Value::Int64(value);
}

void PublishSnapshot()
{
    Json::Value snapshot;
    if (!BuildSnapshot(snapshot)) {
        return;
    }

    const std::string key = SnapshotKey();
    const std::string ttl = std::to_string(kSnapshotTtlSec);
    const std::string now = std::to_string(chen::TimeUtil::GetCurrentSec());
    const std::string cleanup_before = std::to_string(chen::TimeUtil::GetCurrentSec() - kSnapshotTtlSec * 2);

    using RedisArgs = std::vector<std::string>;
    auto rpy =
        chen::RedisUtil::Cmd(GetRedisPoolName(), RedisArgs{"setex", key, ttl, chen::JsonUtil::ToString(snapshot)});
    if (rpy) {
        s_last_publish_sec.store(chen::TimeUtil::GetCurrentSec(), std::memory_order_relaxed);
    }
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
    if (!BuildSnapshot(own)) {
        return false;
    }
    snapshots.push_back({SnapshotKey(), std::move(own)});

    bool degraded = false;
    const std::string min_score = std::to_string(chen::TimeUtil::GetCurrentSec() - kSnapshotTtlSec);
    using RedisArgs = std::vector<std::string>;
    auto index = chen::RedisUtil::Cmd(
        GetRedisPoolName(), RedisArgs{"zrangebyscore", kSnapshotIndexKey, min_score, "+inf"});
    if (!index || index->type != REDIS_REPLY_ARRAY) {
        degraded = true;
        int64_t now = chen::TimeUtil::GetCurrentSec();
        int64_t last_warn = s_last_degraded_warn_sec.load(std::memory_order_relaxed);
        if (now - last_warn >= 60) {
            s_last_degraded_warn_sec.store(now, std::memory_order_relaxed);
            WARN(logger) << "metrics aggregation degraded: redis snapshot index unavailable";
        }
    }
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

    // help 是编译期常量，按指标名去重后单独返回，避免每条 series 重复携带
    Json::Value list(Json::arrayValue);
    Json::Value helps(Json::objectValue);
    for (auto& entry : merged) {
        Json::Value item = std::move(entry.second);
        const std::string name = item["name"].asString();
        if (!helps.isMember(name)) {
            helps[name] = item["help"].asString();
        }
        item.removeMember("help");
        list.append(std::move(item));
    }
    out = Json::Value(Json::objectValue);
    out["metrics"] = std::move(list);
    out["helps"] = std::move(helps);
    out["instance_count"] = snapshots.size();
    out["degraded"] = degraded;
    return true;
}

void RecordWsConnect(bool ok, const std::string& reason)
{
    if (ok) {
        if (s_ws_connect_total) {
            s_ws_connect_total->inc();
        }
        return;
    }

    auto it = s_ws_connect_fail.find(reason);
    if (it == s_ws_connect_fail.end()) {
        it = s_ws_connect_fail.find("internal");
    }
    if (it != s_ws_connect_fail.end() && it->second) {
        it->second->inc();
    }
}

void RecordWsDisconnect()
{
    if (s_ws_disconnect_total) {
        s_ws_disconnect_total->inc();
    }
}
