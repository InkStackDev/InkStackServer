#include "HubClient.hpp"

#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <Chen/Log/Log.hpp>
#include <Chen/Socket/Address.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Config/RpcConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

namespace {

/// 注册失败重试间隔：热重载时 peer_id 会被旧 worker 短暂占用
constexpr uint64_t kRegisterRetryMs = 5000;
constexpr uint64_t kDefaultReconnectMs = 5000;

std::mutex g_mutex;
chen::rpc::RpcClient::ptr g_client;
bool g_registered = false;
uint64_t g_last_connect_attempt_ms = 0;
uint64_t g_last_register_attempt_ms = 0;
HubClient::ConnectedCallback g_connected_cb;

bool ConnectLocked()
{
    const RpcConf conf = GetRpcConf();
    if (!conf.enabled || conf.hub_address.empty()) {
        INFO(logger) << "rpc hub disabled, skip hub client";
        return false;
    }

    auto addr = chen::Address::LookupAny(conf.hub_address);
    if (!addr) {
        ERROR(logger) << "invalid rpc.hub_address: " << conf.hub_address;
        return false;
    }

    auto client = std::make_shared<chen::rpc::RpcClient>();
    client->setHeartbeatInterval(conf.heartbeat_interval_ms);
    if (!client->connect(addr, conf.connect_timeout_ms)) {
        ERROR(logger) << "connect rpc hub failed: " << conf.hub_address;
        return false;
    }

    g_client = client;
    g_registered = false;
    g_last_register_attempt_ms = 0;
    INFO(logger) << "rpc hub connected, hub=" << conf.hub_address << " bus_id=" << conf.bus_id.toString();
    return true;
}

} // namespace

void HubClient::SetConnectedCallback(ConnectedCallback cb)
{
    std::lock_guard lock(g_mutex);
    g_connected_cb = std::move(cb);
}

void HubClient::Init()
{
    if (!GetRpcConf().enabled) {
        INFO(logger) << "rpc hub disabled, skip hub client";
        return;
    }

    chen::rpc::RpcClient::ptr client;
    ConnectedCallback cb;
    bool connected_now = false;
    {
        std::lock_guard lock(g_mutex);
        g_last_connect_attempt_ms = chen::TimeUtil::GetCurrentMs();
        if (!g_client || !g_client->isConnected()) {
            if (g_client) {
                g_client->close();
                g_client.reset();
            }
            g_registered = false;
            connected_now = ConnectLocked();
        }
        client = g_client;
        cb = g_connected_cb;
    }

    // 回调里注册 handler；不能在持锁时调用，避免回调内再进 HubClient
    if (connected_now && cb && client) {
        cb(client);
    }
    Tick();
}

void HubClient::Tick()
{
    if (!GetRpcConf().enabled) {
        return;
    }

    chen::rpc::RpcClient::ptr client;
    ConnectedCallback cb;
    bool connected_now = false;

    {
        std::lock_guard lock(g_mutex);
        if (!g_client || !g_client->isConnected()) {
            const RpcConf conf = GetRpcConf();
            uint64_t interval = conf.reconnect_interval_ms > 0 ? conf.reconnect_interval_ms : kDefaultReconnectMs;
            uint64_t now = chen::TimeUtil::GetCurrentMs();
            if (now < g_last_connect_attempt_ms + interval) {
                return;
            }
            g_last_connect_attempt_ms = now;
            if (g_client) {
                g_client->close();
                g_client.reset();
            }
            g_registered = false;
            if (!ConnectLocked()) {
                return;
            }
            connected_now = true;
        }
        client = g_client;
        cb = g_connected_cb;
    }

    if (connected_now && cb && client) {
        cb(client);
    }

    // 已连接但注册失败（peer_id 被旧 worker 占用）：每 5s 重试
    {
        uint32_t peer_id = 0;
        uint32_t func_id = 0;
        uint32_t instance = 0;
        std::string bus_id;
        {
            std::lock_guard lock(g_mutex);
            if (!g_client || !g_client->isConnected()) {
                g_registered = false;
                return;
            }
            if (g_registered) {
                return;
            }
            uint64_t now = chen::TimeUtil::GetCurrentMs();
            if (g_last_register_attempt_ms != 0 && now < g_last_register_attempt_ms + kRegisterRetryMs) {
                return;
            }
            g_last_register_attempt_ms = now;
            client = g_client;

            const RpcConf conf = GetRpcConf();
            peer_id = conf.bus_id.peerId();
            func_id = conf.bus_id.funcId();
            instance = conf.bus_id.instance;
            bus_id = conf.bus_id.toString();
        }

        bool ok = false;
        try {
            ok = client->registerService(peer_id, func_id, instance, std::vector<uint32_t>{});
        } catch (const std::exception& e) {
            WARN(logger) << "rpc registerService failed: " << e.what();
        }

        {
            std::lock_guard lock(g_mutex);
            if (g_client == client) {
                g_registered = ok;
            }
        }
        if (ok) {
            INFO(logger) << "rpc hub registered, bus_id=" << bus_id;
        } else {
            WARN(logger) << "rpc registerService pending (peer_id occupied by old worker?), bus_id=" << bus_id;
        }
    }
}

void HubClient::Shutdown()
{
    std::lock_guard lock(g_mutex);
    if (g_client) {
        g_client->close();
        g_client.reset();
    }
    g_registered = false;
}

chen::rpc::RpcClient::ptr HubClient::Get()
{
    std::lock_guard lock(g_mutex);
    if (!g_client || !g_client->isConnected()) {
        return nullptr;
    }
    return g_client;
}

bool HubClient::IsRegistered()
{
    std::lock_guard lock(g_mutex);
    return g_registered;
}
