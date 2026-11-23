/**
 * @file HubClient.hpp
 * @brief 进程内唯一的 RPC Hub 客户端（连接、注册、重连）
 * @author Christins
 * @date 2026-11-23
 * @copyright Apache 2.0
 */
#pragma once

#include <functional>

#include <Chen/Rpc/RpcClient.hpp>

/**
 * @brief 共享 Hub 客户端
 * @details 每个进程只维护一条到 hub 的 RpcClient：
 *          连接、心跳、重连、peer 注册全部由本类负责；
 *          SearchClient 等业务调用方统一通过 Get() 取用，不再各自建连。
 */
class HubClient {
public:
    /// 连接建立后的回调（用于注册本地 RPC handler，支持重连后重复调用）
    using ConnectedCallback = std::function<void(const chen::rpc::RpcClient::ptr&)>;

    /// 设置连接成功回调（在 Init 之前调用）
    static void SetConnectedCallback(ConnectedCallback cb);

    /// 首次连接（onServerReady）
    static void Init();

    /// 周期重连 / 补注册（onTick）
    static void Tick();

    /// 关闭连接（onUnload）
    static void Shutdown();

    /// 已连接时返回客户端，否则返回空
    static chen::rpc::RpcClient::ptr Get();

    /// 是否已完成向 hub 的 peer 注册
    static bool IsRegistered();
};
