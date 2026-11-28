/**
 * @file MessageRouter.hpp
 * @brief WebSocket 下行消息路由（单 worker / 跨 worker）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

/**
 * @brief WebSocket 消息路由
 * @details 业务只依赖本接口，不直接碰 WSConnectionRegistry。
 *          连接所在 worker 直接投递；当前 worker 没有目标连接时，通过 Redis 收件箱
 *          转给持有连接的 worker。长连接本身仍由接入的 worker 全程处理，不经过 RPC。
 */
class MessageRouter {
public:
    typedef std::shared_ptr<MessageRouter> ptr;

    virtual ~MessageRouter() = default;

    /**
     * @brief 推送给用户的所有设备
     * @return 0 至少投递/入队成功，-1 用户不在线
     */
    virtual int32_t SendToUser(int64_t user_id, const std::string& message) = 0;

    /**
     * @brief 推送给指定设备
     * @return 0 投递/入队成功，-1 设备不在线
     */
    virtual int32_t SendToDevice(const std::string& device_id, const std::string& message) = 0;

    /**
     * @brief 关闭指定设备连接（跨 worker）
     */
    virtual void CloseDevice(const std::string& device_id, const std::string& message) = 0;

    /**
     * @brief 关闭指定用户的所有连接（跨 worker）
     */
    virtual void CloseUser(int64_t user_id, const std::string& message) = 0;

    /**
     * @brief 广播给所有在线设备
     */
    virtual void Broadcast(const std::string& message) = 0;

    /**
     * @brief 在线用户数（按用户去重）
     */
    virtual int64_t OnlineUserCount() = 0;

    /**
     * @brief 广播在线人数（跨 worker 时由路由决定投递范围）
     */
    virtual void BroadcastPresence() = 0;

    /**
     * @brief 用户是否有任意设备在线
     */
    virtual bool IsUserOnline(int64_t user_id) = 0;

    /**
     * @brief 指定设备是否在线
     */
    virtual bool IsDeviceOnline(const std::string& device_id) = 0;

    /**
     * @brief 连接建立/断开回调（由 WSConnectionRegistry 触发）
     * @param added true 新增，false 断开
     * @param old_user_id 同一 device_id 被替换时的旧用户，0 表示无
     */
    virtual void OnConnection(bool added, int64_t user_id, const std::string& device_id, int64_t old_user_id) = 0;

    virtual void Start() {}
    virtual void Stop() {}

    /**
     * @brief 初始化并安装连接观察者
     */
    static void Init();

    /**
     * @brief 停止并释放当前路由
     */
    static void Shutdown();

    /**
     * @brief 获取当前路由（未初始化时按配置懒初始化）
     */
    static MessageRouter::ptr Get();
};

/// 向用户推送 notification 通道消息
int32_t SendNotificationToUser(int64_t user_id, const std::string& message);

/// 向所有在线用户广播 notification 通道消息
void BroadcastNotification(const std::string& message);

/// 向用户推送 chat 通道消息
int32_t SendChatToUser(int64_t user_id, const std::string& message);
