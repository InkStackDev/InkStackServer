/**
 * @file SearchConfig.hpp
 * @brief 搜索服务（InkStackSearchServer）客户端配置
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Config/Config.hpp>

#include <cstdint>
#include <sstream>
#include <string>

/**
 * @brief 搜索客户端配置（search 节点）
 */
struct SearchConf {
    /// 是否启用搜索客户端
    bool enabled = true;
    /// 搜索服务 RPC hub 地址
    std::string hub_address = "127.0.0.1:9010";
    /// 查询 RPC 超时（毫秒）
    uint64_t timeout_ms = 1500;
    /// 索引更新 RPC 超时（毫秒）
    uint64_t update_timeout_ms = 5000;
    /// 连接超时（毫秒）
    uint64_t connect_timeout_ms = 1000;
    /// 重连间隔（毫秒）
    uint64_t reconnect_interval_ms = 5000;
    /// 脏数据同步间隔（毫秒）
    uint64_t flush_interval_ms = 1000;
    /// 搜索服务状态检查间隔（毫秒）
    uint64_t stats_interval_ms = 30000;
    /// 每批处理的脏 article id 数
    int32_t flush_batch_size = 200;
    /// 全量快照每页文章数
    int32_t snapshot_page_size = 200;
    /// 全量快照每批文档字节数
    uint64_t snapshot_batch_bytes = 524288;
    /// 全量快照 RPC 超时（毫秒）
    uint64_t snapshot_rpc_timeout_ms = 10000;
    /// Redis 脏数据 hash key
    std::string dirty_key = "search:dirty";

    bool operator==(const SearchConf& other) const
    {
        return enabled == other.enabled && hub_address == other.hub_address && timeout_ms == other.timeout_ms
            && update_timeout_ms == other.update_timeout_ms && connect_timeout_ms == other.connect_timeout_ms
            && reconnect_interval_ms == other.reconnect_interval_ms && flush_interval_ms == other.flush_interval_ms
            && stats_interval_ms == other.stats_interval_ms && flush_batch_size == other.flush_batch_size
            && snapshot_page_size == other.snapshot_page_size && snapshot_batch_bytes == other.snapshot_batch_bytes
            && snapshot_rpc_timeout_ms == other.snapshot_rpc_timeout_ms && dirty_key == other.dirty_key;
    }
};

namespace chen {

template <>
class LexicalCast<std::string, SearchConf> {
public:
    SearchConf operator()(const std::string& value)
    {
        SearchConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["enabled"].IsScalar()) {
            conf.enabled = LexicalCast<std::string, bool>()(node["enabled"].Scalar());
        }
        if (node["hub_address"].IsScalar()) {
            conf.hub_address = node["hub_address"].Scalar();
        }
        if (node["timeout_ms"].IsScalar()) {
            conf.timeout_ms = LexicalCast<std::string, uint64_t>()(node["timeout_ms"].Scalar());
        }
        if (node["update_timeout_ms"].IsScalar()) {
            conf.update_timeout_ms = LexicalCast<std::string, uint64_t>()(node["update_timeout_ms"].Scalar());
        }
        if (node["connect_timeout_ms"].IsScalar()) {
            conf.connect_timeout_ms = LexicalCast<std::string, uint64_t>()(node["connect_timeout_ms"].Scalar());
        }
        if (node["reconnect_interval_ms"].IsScalar()) {
            conf.reconnect_interval_ms = LexicalCast<std::string, uint64_t>()(node["reconnect_interval_ms"].Scalar());
        }
        if (node["flush_interval_ms"].IsScalar()) {
            conf.flush_interval_ms = LexicalCast<std::string, uint64_t>()(node["flush_interval_ms"].Scalar());
        }
        if (node["stats_interval_ms"].IsScalar()) {
            conf.stats_interval_ms = LexicalCast<std::string, uint64_t>()(node["stats_interval_ms"].Scalar());
        }
        if (node["flush_batch_size"].IsScalar()) {
            conf.flush_batch_size = LexicalCast<std::string, int32_t>()(node["flush_batch_size"].Scalar());
        }
        if (node["snapshot_page_size"].IsScalar()) {
            conf.snapshot_page_size = LexicalCast<std::string, int32_t>()(node["snapshot_page_size"].Scalar());
        }
        if (node["snapshot_batch_bytes"].IsScalar()) {
            conf.snapshot_batch_bytes = LexicalCast<std::string, uint64_t>()(node["snapshot_batch_bytes"].Scalar());
        }
        if (node["snapshot_rpc_timeout_ms"].IsScalar()) {
            conf.snapshot_rpc_timeout_ms =
                LexicalCast<std::string, uint64_t>()(node["snapshot_rpc_timeout_ms"].Scalar());
        }
        if (node["dirty_key"].IsScalar()) {
            conf.dirty_key = node["dirty_key"].Scalar();
        }
        return conf;
    }
};

template <>
class LexicalCast<SearchConf, std::string> {
public:
    std::string operator()(const SearchConf& conf)
    {
        YAML::Node node;
        node["enabled"] = conf.enabled;
        node["hub_address"] = conf.hub_address;
        node["timeout_ms"] = conf.timeout_ms;
        node["update_timeout_ms"] = conf.update_timeout_ms;
        node["connect_timeout_ms"] = conf.connect_timeout_ms;
        node["reconnect_interval_ms"] = conf.reconnect_interval_ms;
        node["flush_interval_ms"] = conf.flush_interval_ms;
        node["stats_interval_ms"] = conf.stats_interval_ms;
        node["flush_batch_size"] = conf.flush_batch_size;
        node["snapshot_page_size"] = conf.snapshot_page_size;
        node["snapshot_batch_bytes"] = conf.snapshot_batch_bytes;
        node["snapshot_rpc_timeout_ms"] = conf.snapshot_rpc_timeout_ms;
        node["dirty_key"] = conf.dirty_key;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

/**
 * @brief 搜索客户端配置快照（search）
 * @note 每次调用都会读取当前值；热加载后下一次读取即生效
 */
SearchConf GetSearchConf();
