/**
 * @file MetricsService.hpp
 * @brief 服务运行指标注册、路由埋点与快照聚合
 * @author Christins
 * @date 2026-11-08
 * @copyright Apache 2.0
 */
#pragma once

#include <string>
#include <vector>

#include <json/json.h>

#include <Chen/Http/HttpServer.hpp>
#include <Chen/Http/WSServer.hpp>
#include <Chen/Metrics/Metrics.hpp>

/**
 * @brief 单个路由的请求指标
 * @note 只暴露累计量（请求数 / 错误数 / 耗时直方图）。QPS 属于派生值，
 *       由消费方用 requests 与 blog_process_uptime_seconds 自行计算，
 *       不再逐路由注册 gauge，避免快照体积随路由数线性膨胀。
 */
struct RouteMetrics {
    chen::metrics::Counter* requests = nullptr;
    chen::metrics::Counter* errors = nullptr;
    chen::metrics::Histogram* duration = nullptr;
};

/**
 * @brief 注册服务级指标
 * @param name 服务名称
 * @param version 服务版本
 * @param http_servers HTTP 服务器列表，用于采集活跃连接数
 * @param ws_servers WebSocket 服务器列表，用于采集活跃连接数
 */
void Init(
    const std::string& name,
    const std::string& version,
    const std::vector<chen::http::HttpServer::ptr>& http_servers,
    const std::vector<chen::http::WSServer::ptr>& ws_servers);

/**
 * @brief 把本进程指标快照写入 Redis（多实例聚合用）
 * @details key 为 metrics:inst:<instance_id>:<pid>，TTL 60s；同时写入 metrics:instances zset。
 */
void PublishSnapshot();

/**
 * @brief 汇总集群内所有存活实例的指标
 * @details 以本进程 registry 为基准，合并 Redis 中其它实例的快照：
 *          counter / gauge / histogram 默认求和；blog_info 取实例编号最小的一份；
 *          blog_online_users 取最大；系统内存取最大；运行时长取最小。
 *          Redis 不可用或部分快照读取失败时仍返回本进程数据，并置 `degraded=true`。
 * @param[out] out { "metrics": [ ... ], "helps": { name: help }, "instance_count": N, "degraded": bool }
 * @note 每条 metric 不再内联 help（同名指标只保留一份，放顶层 helps），
 *       且零值路由指标会被裁剪，消费方缺失即视作 0。
 */
bool CollectClusterMetrics(Json::Value& out);

/**
 * @brief 记录一次 WebSocket 握手结果
 * @param ok true 表示成功；false 表示失败
 * @param reason 失败原因（no_cookie / not_logged_in / invalid_session / internal）
 */
void RecordWsConnect(bool ok, const std::string& reason);

/**
 * @brief 记录一次 WebSocket 断开
 */
void RecordWsDisconnect();

/**
 * @brief 获取指定路由的指标，首次调用时完成注册
 * @details 指标以 `route` + `method` 两个标签区分：同一路径的 GET / POST / DELETE
 *          是不同 handler，必须独立统计，否则错误率与耗时会互相稀释。
 * @param route 注册路由模板（如 /api/v1/messages/conversations/:id）
 * @param method HTTP 方法字符串（如 GET / POST）
 */
RouteMetrics GetRouteMetrics(const std::string& route, const std::string& method);

/**
 * @brief 记录一次请求
 * @param route_metrics 路由指标
 * @param used_us 请求耗时（微秒）
 * @param err 业务错误码，0 表示成功
 */
void RecordRequest(const RouteMetrics& route_metrics, uint64_t used_us, int32_t err);
