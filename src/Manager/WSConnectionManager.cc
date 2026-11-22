#include "WSConnectionManager.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>

#include "../Message/MessageRouter.hpp"

#include <json/json.h>

static chen::Logger::ptr logger = LOG_ROOT();

std::string BuildChannelMessage(const std::string& message, const std::string& channel)
{
    if (message.empty() || channel.empty()) {
        return message;
    }

    Json::Value root;
    if (!chen::JsonUtil::FromString(root, message) || !root.isObject()) {
        return message;
    }
    root["channel"] = channel;
    return chen::JsonUtil::ToString(root);
}

WSConnectionManager::AddResult WSConnectionManager::addConnection(
    int64_t user_id,
    const std::string& device_id,
    chen::http::WSSession::ptr session,
    bool broadcast_presence)
{
    AddResult result;
    if (user_id <= 0 || device_id.empty() || !session) {
        ERROR(logger) << "WS addConnection invalid param: user_id=" << user_id << " device_id=" << device_id;
        return result;
    }

    bool user_became_online = false;
    bool old_user_became_offline = false;
    int64_t old_user_id = 0;

    {
        std::unique_lock lock(mutex_);

        auto dit = devices_.find(device_id);
        if (dit != devices_.end()) {
            auto old_conn = dit->second;
            if (old_conn && old_conn->session == session) {
                // 同一个连接重复注册，直接返回
                return result;
            }
            if (old_conn) {
                result.replaced_session = old_conn->session;
                old_user_id = old_conn->user_id;
                // 暂不移除旧 session 映射，Gateway 还需要通过它发送 replaced 帧；
                // 旧连接 onClose 时 removeConnection 会清理。
                auto old_uit = user_devices_.find(old_conn->user_id);
                if (old_uit != user_devices_.end()) {
                    old_uit->second.erase(device_id);
                    if (old_uit->second.empty()) {
                        user_devices_.erase(old_uit);
                        old_user_became_offline = true;
                    }
                }
            }
            devices_.erase(dit);
        }

        auto conn = std::make_shared<Connection>();
        conn->user_id = user_id;
        conn->device_id = device_id;
        conn->session = session;

        auto& devices = user_devices_[user_id];
        user_became_online = devices.empty();
        devices.insert(device_id);
        devices_[device_id] = conn;
        session_devices_[session.get()] = conn;
    }

    result.replaced_user_id = old_user_id;
    notifyConnection(true, user_id, device_id, old_user_id);

    if (user_became_online || old_user_became_offline) {
        result.presence_changed = true;
        if (broadcast_presence) {
            MessageRouter::Get()->BroadcastPresence();
        }
    }

    result.user_became_online = user_became_online;
    if (result.replaced_session) {
        INFO(logger) << "WS replace: user_id=" << user_id << " device=" << device_id.substr(0, 12);
    } else {
        INFO(logger) << "WS connected: user_id=" << user_id << " device=" << device_id.substr(0, 12);
    }
    return result;
}

WSConnectionManager::RemoveResult WSConnectionManager::removeConnection(chen::http::WSSession::ptr session)
{
    RemoveResult result;
    if (!session) {
        return result;
    }

    {
        std::unique_lock lock(mutex_);

        auto sit = session_devices_.find(session.get());
        if (sit == session_devices_.end()) {
            return result;
        }

        auto conn = sit->second;
        if (!conn) {
            session_devices_.erase(sit);
            return result;
        }

        auto dit = devices_.find(conn->device_id);
        if (dit == devices_.end() || dit->second != conn) {
            // 该连接已被同设备的新连接顶掉，只清理旧 session 映射
            session_devices_.erase(sit);
            return result;
        }

        session_devices_.erase(sit);
        devices_.erase(dit);

        result.removed = true;
        result.user_id = conn->user_id;
        result.device_id = conn->device_id;

        auto uit = user_devices_.find(conn->user_id);
        if (uit != user_devices_.end()) {
            uit->second.erase(conn->device_id);
            if (uit->second.empty()) {
                user_devices_.erase(uit);
                result.user_became_offline = true;
            }
        }
    }

    if (result.removed) {
        notifyConnection(false, result.user_id, result.device_id, 0);
    }
    if (result.user_became_offline) {
        MessageRouter::Get()->BroadcastPresence();
    }

    INFO(logger) << "WS disconnected: user_id=" << result.user_id;
    return result;
}

bool WSConnectionManager::getIdentity(chen::http::WSSession::ptr session, int64_t& user_id, std::string& device_id) const
{
    if (!session) {
        return false;
    }

    std::shared_lock lock(mutex_);
    auto sit = session_devices_.find(session.get());
    if (sit == session_devices_.end() || !sit->second) {
        return false;
    }

    auto conn = sit->second;
    auto dit = devices_.find(conn->device_id);
    if (dit == devices_.end() || dit->second != conn) {
        return false;
    }

    user_id = conn->user_id;
    device_id = conn->device_id;
    return true;
}

int32_t WSConnectionManager::sendToUser(int64_t user_id, const std::string& message)
{
    std::vector<Connection::ptr> conns;
    {
        std::shared_lock lock(mutex_);
        auto uit = user_devices_.find(user_id);
        if (uit == user_devices_.end()) {
            return -1;
        }

        conns.reserve(uit->second.size());
        for (const auto& device_id : uit->second) {
            auto dit = devices_.find(device_id);
            if (dit != devices_.end() && dit->second) {
                conns.push_back(dit->second);
            }
        }
    }

    bool sent = false;
    for (const auto& conn : conns) {
        if (SendToConnection(conn, message) == 0) {
            sent = true;
        }
    }
    return sent ? 0 : -1;
}

int32_t WSConnectionManager::sendToSession(chen::http::WSSession::ptr session, const std::string& message)
{
    if (!session) {
        return -1;
    }

    Connection::ptr conn;
    {
        std::shared_lock lock(mutex_);
        auto sit = session_devices_.find(session.get());
        if (sit == session_devices_.end() || !sit->second) {
            return -1;
        }
        conn = sit->second;
    }

    return SendToConnection(conn, message);
}

int32_t WSConnectionManager::sendToDevice(const std::string& device_id, const std::string& message)
{
    Connection::ptr conn;
    {
        std::shared_lock lock(mutex_);
        auto dit = devices_.find(device_id);
        if (dit == devices_.end() || !dit->second) {
            return -1;
        }
        conn = dit->second;
    }

    return SendToConnection(conn, message);
}

void WSConnectionManager::broadcast(const std::string& message)
{
    std::vector<Connection::ptr> conns;
    {
        std::shared_lock lock(mutex_);
        conns.reserve(devices_.size());
        for (const auto& item : devices_) {
            if (item.second) {
                conns.push_back(item.second);
            }
        }
    }

    for (const auto& conn : conns) {
        SendToConnection(conn, message);
    }
}

bool WSConnectionManager::isUserOnline(int64_t user_id) const
{
    if (user_id <= 0) {
        return false;
    }

    std::shared_lock lock(mutex_);
    auto uit = user_devices_.find(user_id);
    if (uit == user_devices_.end()) {
        return false;
    }

    for (const auto& device_id : uit->second) {
        auto dit = devices_.find(device_id);
        if (dit != devices_.end() && dit->second && dit->second->session && dit->second->session->isConnected()) {
            return true;
        }
    }
    return false;
}

bool WSConnectionManager::isDeviceOnline(const std::string& device_id) const
{
    if (device_id.empty()) {
        return false;
    }

    std::shared_lock lock(mutex_);
    auto dit = devices_.find(device_id);
    return dit != devices_.end() && dit->second && dit->second->session && dit->second->session->isConnected();
}

int32_t WSConnectionManager::onlineUserCount() const
{
    std::shared_lock lock(mutex_);
    return static_cast<int32_t>(user_devices_.size());
}

int32_t WSConnectionManager::connectionCount() const
{
    std::shared_lock lock(mutex_);
    return static_cast<int32_t>(devices_.size());
}

bool WSConnectionManager::closeSession(chen::http::WSSession::ptr session, const std::string& notify_message)
{
    if (!session) {
        return false;
    }

    Connection::ptr conn;
    {
        std::shared_lock lock(mutex_);
        auto sit = session_devices_.find(session.get());
        if (sit == session_devices_.end() || !sit->second) {
            return false;
        }
        conn = sit->second;
    }

    CloseConnection(conn, notify_message);
    return true;
}

bool WSConnectionManager::closeDevice(const std::string& device_id, const std::string& notify_message)
{
    if (device_id.empty()) {
        return false;
    }

    Connection::ptr conn;
    bool user_became_offline = false;
    {
        std::unique_lock lock(mutex_);
        auto dit = devices_.find(device_id);
        if (dit == devices_.end()) {
            return false;
        }

        conn = dit->second;
        devices_.erase(dit);
        if (conn && conn->session) {
            session_devices_.erase(conn->session.get());
        }
        if (conn) {
            auto uit = user_devices_.find(conn->user_id);
            if (uit != user_devices_.end()) {
                uit->second.erase(device_id);
                if (uit->second.empty()) {
                    user_devices_.erase(uit);
                    user_became_offline = true;
                }
            }
        }
    }

    CloseConnection(conn, notify_message);

    if (conn) {
        notifyConnection(false, conn->user_id, device_id, 0);
    }
    if (user_became_offline) {
        MessageRouter::Get()->BroadcastPresence();
    }

    INFO(logger) << "WS close device: " << device_id.substr(0, 12);
    return true;
}

bool WSConnectionManager::closeUser(int64_t user_id, const std::string& notify_message)
{
    if (user_id <= 0) {
        return false;
    }

    std::vector<Connection::ptr> conns;
    {
        std::unique_lock lock(mutex_);
        auto uit = user_devices_.find(user_id);
        if (uit == user_devices_.end()) {
            return false;
        }

        for (const auto& device_id : uit->second) {
            auto dit = devices_.find(device_id);
            if (dit == devices_.end() || !dit->second) {
                continue;
            }
            auto conn = dit->second;
            if (conn->session) {
                session_devices_.erase(conn->session.get());
            }
            conns.push_back(conn);
            devices_.erase(dit);
        }
        user_devices_.erase(uit);
    }

    for (const auto& conn : conns) {
        CloseConnection(conn, notify_message);
        if (conn) {
            notifyConnection(false, conn->user_id, conn->device_id, 0);
        }
    }

    if (!conns.empty()) {
        MessageRouter::Get()->BroadcastPresence();
    }

    INFO(logger) << "WS close user: user_id=" << user_id << " devices=" << conns.size();
    return !conns.empty();
}

int32_t WSConnectionManager::closeAllConnections(const std::string& notify_message)
{
    std::vector<Connection::ptr> conns;
    {
        std::unique_lock lock(mutex_);
        conns.reserve(devices_.size());
        for (const auto& item : devices_) {
            if (item.second) {
                conns.push_back(item.second);
            }
        }
        devices_.clear();
        user_devices_.clear();
        session_devices_.clear();
    }

    for (const auto& conn : conns) {
        CloseConnection(conn, notify_message);
        if (conn) {
            notifyConnection(false, conn->user_id, conn->device_id, 0);
        }
    }

    if (!conns.empty()) {
        MessageRouter::Get()->BroadcastPresence();
    }

    INFO(logger) << "WS close all connections: " << conns.size();
    return static_cast<int32_t>(conns.size());
}

void WSConnectionManager::CloseConnection(const Connection::ptr& conn, const std::string& notify_message)
{
    if (!conn || !conn->session) {
        return;
    }

    std::lock_guard lock(conn->send_mutex);
    if (!notify_message.empty() && conn->session->isConnected()) {
        conn->session->sendMessage(notify_message);
    }
    conn->session->close();
}

int32_t WSConnectionManager::SendToConnection(const Connection::ptr& conn, const std::string& message)
{
    if (!conn || !conn->session || !conn->session->isConnected()) {
        return -1;
    }

    std::lock_guard lock(conn->send_mutex);
    if (!conn->session->isConnected()) {
        return -1;
    }
    // 框架返回写入字节数，统一归一化为 0 成功 / -1 失败
    return conn->session->sendMessage(message) < 0 ? -1 : 0;
}

void WSConnectionManager::broadcastPresence()
{
    Json::Value msg;
    msg["channel"] = "presence";
    msg["type"] = "online_count";
    msg["count"] = static_cast<Json::Int64>(MessageRouter::Get()->OnlineUserCount());
    broadcast(chen::JsonUtil::ToString(msg));
}

void WSConnectionManager::setConnectionObserver(ConnectionObserver observer)
{
    std::unique_lock lock(mutex_);
    observer_ = std::move(observer);
}

void WSConnectionManager::snapshot(std::vector<std::pair<int64_t, std::string>>& out) const
{
    std::shared_lock lock(mutex_);
    out.clear();
    out.reserve(devices_.size());
    for (const auto& item : devices_) {
        if (item.second) {
            out.emplace_back(item.second->user_id, item.first);
        }
    }
}

void WSConnectionManager::notifyConnection(bool added,
    int64_t user_id,
    const std::string& device_id,
    int64_t old_user_id) const
{
    ConnectionObserver observer;
    {
        std::shared_lock lock(mutex_);
        observer = observer_;
    }
    if (observer) {
        observer(added, user_id, device_id, old_user_id);
    }
}
