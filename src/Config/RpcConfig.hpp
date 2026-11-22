/**
 * @file RpcConfig.hpp
 * @brief RPC Hub 客户端配置（总线身份 / hub 地址 / 超时）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Config/Config.hpp>
#include <Chen/Util/StringUtil.hpp>
#include <Chen/Util/Macro.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief RPC 总线身份
 * @details 四段位 `<zone>.<group>.<service>.<instance>`：
 *          - peerId = zone<<24 | group<<16 | service<<8 | instance，按 hub 全局唯一
 *          - funcId = group<<8 | service，同一服务的所有实例共享，供 GROUPID / BROADCAST 路由
 *          - 各段取值范围 1..255
 */
struct RpcBusId {
    uint32_t zone = 1;
    uint32_t group = 1;
    uint32_t service = 1;
    uint32_t instance = 1;

    /// 打包为 hub 侧的 peer_id
    uint32_t peerId() const { return (zone << 24) | (group << 16) | (service << 8) | instance; }

    /// 打包为 hub 侧的功能类型
    uint32_t funcId() const { return (group << 8) | service; }

    std::string toString() const
    {
        std::vector parts = {
            std::to_string(zone), std::to_string(group), std::to_string(service), std::to_string(instance)};
        return chen::StringUtil::Join(parts, ".");
    }

    bool operator==(const RpcBusId& other) const
    {
        return zone == other.zone && group == other.group && service == other.service && instance == other.instance;
    }
};

/**
 * @brief RPC Hub 客户端配置（rpc 节点）
 */
struct RpcConf {
    /// 是否启用 hub 客户端
    bool enabled = true;
    /// hub 地址；为空表示未配置，连接层据此跳过
    std::string hub_address;
    /// 本实例在总线上的身份
    RpcBusId bus_id;
    /// 请求级超时（毫秒）
    uint64_t timeout_ms = 3000;
    /// 连接 hub 超时（毫秒）
    uint64_t connect_timeout_ms = 1000;
    /// 断线重连间隔（毫秒）
    uint64_t reconnect_interval_ms = 5000;
    /// 心跳间隔（毫秒）
    uint64_t heartbeat_interval_ms = 30000;

    std::string getHubIP() const
    {
        std::vector parts = chen::StringUtil::Split(hub_address, ':', true);
        ASSERT_RET(parts.size() == 2, "");

        return parts.front();
    }

    uint16_t getHubPort() const
    {
        std::vector parts = chen::StringUtil::Split(hub_address, ':', true);
        ASSERT_RET(parts.size() == 2, 0);

        return atoi(parts.back().c_str());
        ∑ =
    }

    bool operator==(const RpcConf& other) const
    {
        return enabled == other.enabled && hub_address == other.hub_address && bus_id == other.bus_id
            && timeout_ms == other.timeout_ms && connect_timeout_ms == other.connect_timeout_ms
            && reconnect_interval_ms == other.reconnect_interval_ms
            && heartbeat_interval_ms == other.heartbeat_interval_ms;
    }
};

namespace chen {

template <>
class LexicalCast<std::string, RpcBusId> {
public:
    RpcBusId operator()(const std::string& value)
    {
        const std::vector<std::string> parts = StringUtil::Split(value, '.', false);
        if (parts.size() != 4) {
            throw std::invalid_argument(
                "invalid bus_id '" + value + "', expect <zone>.<group>.<service>.<instance>, each in 1..255");
        }

        uint8_t fields[4] = {0, 0, 0, 0};
        for (size_t i = 0; i < 4; ++i) {
            int32_t field = 0;
            try {
                field = LexicalCast<std::string, int32_t>()(parts[i]);
            } catch (const std::exception&) {
                throw std::invalid_argument(
                    "invalid bus_id '" + value + "', segment '" + parts[i] + "' is not a number");
            }
            if (field < 1 || field > 255) {
                throw std::invalid_argument(
                    "invalid bus_id '" + value + "', segment '" + parts[i] + "' out of range 1..255");
            }
            fields[i] = static_cast<uint8_t>(field);
        }

        RpcBusId bus_id;
        bus_id.zone = fields[0];
        bus_id.group = fields[1];
        bus_id.service = fields[2];
        bus_id.instance = fields[3];
        return bus_id;
    }
};

template <>
class LexicalCast<RpcBusId, std::string> {
public:
    std::string operator()(const RpcBusId& bus_id) { return bus_id.toString(); }
};

template <>
class LexicalCast<std::string, RpcConf> {
public:
    RpcConf operator()(const std::string& value)
    {
        RpcConf conf;
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
        if (node["bus_id"].IsScalar()) {
            conf.bus_id = LexicalCast<std::string, RpcBusId>()(node["bus_id"].Scalar());
        }
        if (node["timeout_ms"].IsScalar()) {
            conf.timeout_ms = LexicalCast<std::string, uint64_t>()(node["timeout_ms"].Scalar());
        }
        if (node["connect_timeout_ms"].IsScalar()) {
            conf.connect_timeout_ms = LexicalCast<std::string, uint64_t>()(node["connect_timeout_ms"].Scalar());
        }
        if (node["reconnect_interval_ms"].IsScalar()) {
            conf.reconnect_interval_ms = LexicalCast<std::string, uint64_t>()(node["reconnect_interval_ms"].Scalar());
        }
        if (node["heartbeat_interval_ms"].IsScalar()) {
            conf.heartbeat_interval_ms = LexicalCast<std::string, uint64_t>()(node["heartbeat_interval_ms"].Scalar());
        }
        return conf;
    }
};

template <>
class LexicalCast<RpcConf, std::string> {
public:
    std::string operator()(const RpcConf& conf)
    {
        YAML::Node node;
        node["enabled"] = conf.enabled;
        node["hub_address"] = conf.hub_address;
        node["bus_id"] = conf.bus_id.toString();
        node["timeout_ms"] = conf.timeout_ms;
        node["connect_timeout_ms"] = conf.connect_timeout_ms;
        node["reconnect_interval_ms"] = conf.reconnect_interval_ms;
        node["heartbeat_interval_ms"] = conf.heartbeat_interval_ms;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

/**
 * @brief RPC Hub 客户端配置快照（rpc）
 * @note 每次调用都会读取当前值；热加载后下一次读取即生效
 */
RpcConf GetRpcConf();
