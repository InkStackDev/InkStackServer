#include "MessageRouter.hpp"

#include <unistd.h>

#include <atomic>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

#include <json/json.h>

#include <Chen/DB/Redis.hpp>
#include <Chen/IOManager/IOManager.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Timer/Timer.hpp>
#include <Chen/Util/JsonUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>
#include <Chen/Util/TypeUtil.hpp>

#include "../Config/AppConfig.hpp"
#include "WSConnectionRegistry.hpp"
#include "WSMessage.hpp"
#include "../Util/FiberMutex.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

constexpr int kPollIntervalMs = 50;
constexpr int kMaxPollPerTick = 200;
constexpr int kHeartbeatIntervalMs = 10000;
constexpr int kWorkerTtlSec = 30;
constexpr int kDeviceTtlSec = 120;
constexpr int kInboxTtlSec = 120;
constexpr int kWorkerDevicesTtlSec = 3600;

static const char* kWorkersKey = "ws:workers";
static const char* kOnlineUsersKey = "ws:online:users";

static std::string UserKey(int64_t uid)
{
    return "ws:user:" + std::to_string(uid);
}

static std::string DeviceKey(const std::string& device_id)
{
    return "ws:device:" + device_id;
}

static std::string WorkerDevicesKey(const std::string& worker_id)
{
    return "ws:worker_devices:" + worker_id;
}

static std::string InboxKey(const std::string& worker_id)
{
    return "ws:inbox:" + worker_id;
}

static std::string Member(int64_t uid, const std::string& device_id)
{
    return std::to_string(uid) + ":" + device_id;
}

static bool GetString(const std::string& key, std::string& out)
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", key.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING || !rpy->str) {
        return false;
    }
    out.assign(rpy->str, rpy->len);
    return true;
}

static int64_t GetCard(const std::string& key)
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "scard %s", key.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_INTEGER) {
        return -1;
    }
    return rpy->integer;
}

static bool DelIfEquals(const std::string& key, const std::string& expected)
{
    static const char* kScript = "if redis.call('get', KEYS[1]) == ARGV[1] then "
                                 "return redis.call('del', KEYS[1]) end return 0";
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "eval %s 1 %s %s", kScript, key.c_str(), expected.c_str());
    return rpy && rpy->type == REDIS_REPLY_INTEGER && rpy->integer > 0;
}

static Json::Value MakePayload(const std::string& kind,
    int64_t uid,
    const std::string& device_id,
    const std::string& message)
{
    Json::Value v;
    v["k"] = kind;
    if (uid > 0) {
        v["uid"] = static_cast<Json::Int64>(uid);
    }
    if (!device_id.empty()) {
        v["device"] = device_id;
    }
    v["msg"] = message;
    return v;
}

static bool ParseMember(const std::string& value, int64_t& uid, std::string& device_id)
{
    auto pos = value.find(':');
    if (pos == std::string::npos || pos == 0 || pos + 1 >= value.size()) {
        return false;
    }
    uid = chen::TypeUtil::Atoi(value.substr(0, pos));
    if (uid <= 0) {
        return false;
    }
    device_id = value.substr(pos + 1);
    return !device_id.empty();
}

class MessageRouterRedis : public MessageRouter {
public:
    MessageRouterRedis()
        : worker_id_(std::to_string(GetServerInstanceId()) + ":" + std::to_string(::getpid()))
    {
    }

    void Start() override
    {
        if (started_.exchange(true)) {
            return;
        }
        auto iom = chen::IOManager::GetThis();
        if (!iom) {
            ERROR(logger) << "redis message router start failed: no IOManager";
            started_.store(false);
            return;
        }
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", InboxKey(worker_id_).c_str());
        Heartbeat();
        inbox_timer_ = iom->addTimer(
            kPollIntervalMs,
            [this]() {
                PollInbox();
            },
            true);
        heartbeat_timer_ = iom->addTimer(
            kHeartbeatIntervalMs,
            [this]() {
                Heartbeat();
            },
            true);
        INFO(logger) << "redis message router started, worker=" << worker_id_;
    }

    void Stop() override
    {
        if (!started_.exchange(false)) {
            return;
        }
        if (inbox_timer_) {
            inbox_timer_->cancel();
            inbox_timer_.reset();
        }
        if (heartbeat_timer_) {
            heartbeat_timer_->cancel();
            heartbeat_timer_.reset();
        }
        INFO(logger) << "redis message router stopped, worker=" << worker_id_;
    }

    int32_t SendToUser(int64_t user_id, const std::string& message) override
    {
        if (!started_.load()) {
            return WSRegistry::GetInstance()->sendToUser(user_id, message);
        }

        int32_t local = WSRegistry::GetInstance()->sendToUser(user_id, message);
        bool queued = false;

        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "smembers %s", UserKey(user_id).c_str());
        if (rpy && rpy->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < rpy->elements; ++i) {
                if (!rpy->element[i] || !rpy->element[i]->str) {
                    continue;
                }
                std::string device_id(rpy->element[i]->str, rpy->element[i]->len);
                std::string owner;
                if (!GetString(DeviceKey(device_id), owner)) {
                    RemoveUserDevice(user_id, device_id);
                    continue;
                }
                if (owner == worker_id_) {
                    continue;
                }
                if (!WorkerAlive(owner)) {
                    RemovePresence(user_id, device_id, owner);
                    continue;
                }
                if (Enqueue(owner, MakePayload("user", user_id, "", message))) {
                    queued = true;
                }
            }
        }
        return (local == 0 || queued) ? 0 : -1;
    }

    int32_t SendToDevice(const std::string& device_id, const std::string& message) override
    {
        if (!started_.load()) {
            return WSRegistry::GetInstance()->sendToDevice(device_id, message);
        }
        if (WSRegistry::GetInstance()->sendToDevice(device_id, message) == 0) {
            return 0;
        }

        std::string owner;
        if (!GetString(DeviceKey(device_id), owner)) {
            return -1;
        }
        if (owner == worker_id_ || !WorkerAlive(owner)) {
            if (owner != worker_id_) {
                RemoveDevice(device_id, owner);
            }
            return -1;
        }
        return Enqueue(owner, MakePayload("device", 0, device_id, message)) ? 0 : -1;
    }

    void CloseDevice(const std::string& device_id, const std::string& message) override
    {
        if (WSRegistry::GetInstance()->closeDevice(device_id, message)) {
            return;
        }
        if (!started_.load()) {
            return;
        }

        std::string owner;
        if (!GetString(DeviceKey(device_id), owner)) {
            return;
        }
        if (owner == worker_id_) {
            return;
        }
        if (!WorkerAlive(owner)) {
            RemoveDevice(device_id, owner);
            return;
        }
        Enqueue(owner, MakePayload("close_device", 0, device_id, message));
    }

    void CloseUser(int64_t user_id, const std::string& message) override
    {
        WSRegistry::GetInstance()->closeUser(user_id, message);
        if (!started_.load()) {
            return;
        }

        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "smembers %s", UserKey(user_id).c_str());
        if (!rpy || rpy->type != REDIS_REPLY_ARRAY) {
            return;
        }
        std::set<std::string> workers;
        for (size_t i = 0; i < rpy->elements; ++i) {
            if (!rpy->element[i] || !rpy->element[i]->str) {
                continue;
            }
            std::string device_id(rpy->element[i]->str, rpy->element[i]->len);
            std::string owner;
            if (!GetString(DeviceKey(device_id), owner)) {
                RemoveUserDevice(user_id, device_id);
                continue;
            }
            if (owner == worker_id_) {
                continue;
            }
            if (!WorkerAlive(owner)) {
                RemovePresence(user_id, device_id, owner);
                continue;
            }
            workers.insert(owner);
        }
        for (const auto& worker : workers) {
            Enqueue(worker, MakePayload("close_user", user_id, "", message));
        }
    }

    void Broadcast(const std::string& message) override
    {
        WSRegistry::GetInstance()->broadcast(message);
        if (!started_.load()) {
            return;
        }

        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "zrange %s 0 -1", kWorkersKey);
        if (!rpy || rpy->type != REDIS_REPLY_ARRAY) {
            return;
        }
        for (size_t i = 0; i < rpy->elements; ++i) {
            if (!rpy->element[i] || !rpy->element[i]->str) {
                continue;
            }
            std::string id(rpy->element[i]->str, rpy->element[i]->len);
            if (id == worker_id_) {
                continue;
            }
            if (!WorkerAlive(id)) {
                CleanupWorker(id);
                continue;
            }
            Enqueue(id, MakePayload("broadcast", 0, "", message));
        }
    }

    int64_t OnlineUserCount() override
    {
        int64_t count = GetCard(kOnlineUsersKey);
        if (count >= 0) {
            return count;
        }
        return WSRegistry::GetInstance()->onlineUserCount();
    }

    void BroadcastPresence() override
    {
        Json::Value msg;
        msg["channel"] = "presence";
        msg["type"] = "online_count";
        msg["count"] = static_cast<Json::Int64>(OnlineUserCount());
        Broadcast(chen::JsonUtil::ToString(msg));
    }

    bool IsUserOnline(int64_t user_id) override
    {
        if (!started_.load() || WSRegistry::GetInstance()->isUserOnline(user_id)) {
            return WSRegistry::GetInstance()->isUserOnline(user_id);
        }
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "smembers %s", UserKey(user_id).c_str());
        if (!rpy || rpy->type != REDIS_REPLY_ARRAY) {
            return WSRegistry::GetInstance()->isUserOnline(user_id);
        }
        for (size_t i = 0; i < rpy->elements; ++i) {
            if (!rpy->element[i] || !rpy->element[i]->str) {
                continue;
            }
            std::string device_id(rpy->element[i]->str, rpy->element[i]->len);
            std::string owner;
            if (!GetString(DeviceKey(device_id), owner)) {
                RemoveUserDevice(user_id, device_id);
                continue;
            }
            if (owner == worker_id_ || WorkerAlive(owner)) {
                return true;
            }
            RemovePresence(user_id, device_id, owner);
        }
        return false;
    }

    bool IsDeviceOnline(const std::string& device_id) override
    {
        if (!started_.load() || WSRegistry::GetInstance()->isDeviceOnline(device_id)) {
            return WSRegistry::GetInstance()->isDeviceOnline(device_id);
        }
        std::string owner;
        if (!GetString(DeviceKey(device_id), owner)) {
            return WSRegistry::GetInstance()->isDeviceOnline(device_id);
        }
        if (owner == worker_id_) {
            return WSRegistry::GetInstance()->isDeviceOnline(device_id);
        }
        return WorkerAlive(owner);
    }

    void OnConnection(bool added, int64_t user_id, const std::string& device_id, int64_t old_user_id) override
    {
        if (!started_.load()) {
            return;
        }
        if (added) {
            if (old_user_id > 0 && old_user_id != user_id) {
                chen::RedisUtil::Cmd(GetRedisPoolName(), "srem %s %s", WorkerDevicesKey(worker_id_).c_str(),
                    Member(old_user_id, device_id).c_str());
                RemoveUserDevice(old_user_id, device_id);
            }
            AddPresence(user_id, device_id);
        } else {
            RemovePresence(user_id, device_id, worker_id_);
        }
    }

private:
    void AddPresence(int64_t user_id, const std::string& device_id)
    {
        chen::RedisUtil::Cmd(GetRedisPoolName(), "sadd %s %s", UserKey(user_id).c_str(), device_id.c_str());
        chen::RedisUtil::Cmd(
            GetRedisPoolName(), "sadd %s %s", WorkerDevicesKey(worker_id_).c_str(), Member(user_id, device_id).c_str());
        chen::RedisUtil::Cmd(
            GetRedisPoolName(), "set %s %s ex %d", DeviceKey(device_id).c_str(), worker_id_.c_str(), kDeviceTtlSec);
        chen::RedisUtil::Cmd(GetRedisPoolName(), "sadd %s %lld", kOnlineUsersKey, static_cast<long long>(user_id));
    }

    void RemoveUserDevice(int64_t user_id, const std::string& device_id)
    {
        chen::RedisUtil::Cmd(GetRedisPoolName(), "srem %s %s", UserKey(user_id).c_str(), device_id.c_str());
        if (GetCard(UserKey(user_id)) == 0) {
            chen::RedisUtil::Cmd(GetRedisPoolName(), "srem %s %lld", kOnlineUsersKey, static_cast<long long>(user_id));
        }
    }

    void RemovePresence(int64_t user_id, const std::string& device_id, const std::string& owner_id)
    {
        std::string current_owner;
        if (GetString(DeviceKey(device_id), current_owner) && !owner_id.empty() && current_owner != owner_id) {
            // 设备已经迁移到新的 worker，不能删掉新连接的注册信息
            return;
        }

        chen::RedisUtil::Cmd(GetRedisPoolName(), "srem %s %s", UserKey(user_id).c_str(), device_id.c_str());
        if (!owner_id.empty()) {
            chen::RedisUtil::Cmd(GetRedisPoolName(), "srem %s %s", WorkerDevicesKey(owner_id).c_str(),
                Member(user_id, device_id).c_str());
            DelIfEquals(DeviceKey(device_id), owner_id);
        }
        if (GetCard(UserKey(user_id)) == 0) {
            chen::RedisUtil::Cmd(GetRedisPoolName(), "srem %s %lld", kOnlineUsersKey, static_cast<long long>(user_id));
        }
    }

    void RemoveDevice(const std::string& device_id, const std::string& owner_id)
    {
        DelIfEquals(DeviceKey(device_id), owner_id);
    }

    bool WorkerAlive(const std::string& worker_id)
    {
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "zscore %s %s", kWorkersKey, worker_id.c_str());
        return rpy && rpy->type == REDIS_REPLY_STRING;
    }

    void CleanupWorker(const std::string& worker_id)
    {
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "smembers %s", WorkerDevicesKey(worker_id).c_str());
        if (rpy && rpy->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < rpy->elements; ++i) {
                if (!rpy->element[i] || !rpy->element[i]->str) {
                    continue;
                }
                int64_t uid = 0;
                std::string device_id;
                if (!ParseMember(std::string(rpy->element[i]->str, rpy->element[i]->len), uid, device_id)) {
                    continue;
                }
                RemovePresence(uid, device_id, worker_id);
            }
        }
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", WorkerDevicesKey(worker_id).c_str());
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", InboxKey(worker_id).c_str());
        chen::RedisUtil::Cmd(GetRedisPoolName(), "zrem %s %s", kWorkersKey, worker_id.c_str());
    }

    void Heartbeat()
    {
        if (!started_.load()) {
            return;
        }
        int64_t now = chen::TimeUtil::GetCurrentSec();
        chen::RedisUtil::Cmd(
            GetRedisPoolName(), "zadd %s %lld %s", kWorkersKey, static_cast<long long>(now), worker_id_.c_str());

        std::vector<std::pair<int64_t, std::string>> devices;
        WSRegistry::GetInstance()->snapshot(devices);
        for (const auto& item : devices) {
            AddPresence(item.first, item.second);
        }

        chen::RedisUtil::Cmd(
            GetRedisPoolName(), "expire %s %d", WorkerDevicesKey(worker_id_).c_str(), kWorkerDevicesTtlSec);
        chen::RedisUtil::Cmd(GetRedisPoolName(), "expire %s %d", InboxKey(worker_id_).c_str(), kInboxTtlSec);

        auto stale = chen::RedisUtil::Cmd(GetRedisPoolName(), "zrangebyscore %s -inf %lld", kWorkersKey, now - kWorkerTtlSec);
        if (stale && stale->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < stale->elements; ++i) {
                if (!stale->element[i] || !stale->element[i]->str) {
                    continue;
                }
                std::string id(stale->element[i]->str, stale->element[i]->len);
                if (id != worker_id_) {
                    CleanupWorker(id);
                }
            }
        }
        chen::RedisUtil::Cmd(GetRedisPoolName(), "zremrangebyscore %s -inf %lld", kWorkersKey, now - kWorkerTtlSec);
    }

    bool Enqueue(const std::string& worker_id, const Json::Value& payload)
    {
        std::string data = chen::JsonUtil::ToString(payload);
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "rpush %s %s", InboxKey(worker_id).c_str(), data.c_str());
        if (!rpy || rpy->type != REDIS_REPLY_INTEGER) {
            return false;
        }
        chen::RedisUtil::Cmd(GetRedisPoolName(), "expire %s %d", InboxKey(worker_id).c_str(), kInboxTtlSec);
        return true;
    }

    void PollInbox()
    {
        if (!started_.load()) {
            return;
        }
        for (int i = 0; i < kMaxPollPerTick; ++i) {
            auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "lpop %s", InboxKey(worker_id_).c_str());
            if (!rpy || rpy->type != REDIS_REPLY_STRING || !rpy->str) {
                break;
            }
            Json::Value root;
            if (!chen::JsonUtil::FromString(root, std::string(rpy->str, rpy->len)) || !root.isObject()) {
                WARN(logger) << "invalid ws inbox payload, worker=" << worker_id_;
                continue;
            }
            std::string kind = root["k"].asString();
            std::string message = root["msg"].asString();
            if (kind == "user") {
                WSRegistry::GetInstance()->sendToUser(root["uid"].asInt64(), message);
            } else if (kind == "device") {
                WSRegistry::GetInstance()->sendToDevice(root["device"].asString(), message);
            } else if (kind == "broadcast") {
                WSRegistry::GetInstance()->broadcast(message);
            } else if (kind == "close_device") {
                WSRegistry::GetInstance()->closeDevice(root["device"].asString(), message);
            } else if (kind == "close_user") {
                WSRegistry::GetInstance()->closeUser(root["uid"].asInt64(), message);
            }
        }
    }

private:
    std::string worker_id_;
    std::atomic<bool> started_{false};
    chen::Timer::ptr inbox_timer_;
    chen::Timer::ptr heartbeat_timer_;
};

static FiberMutex g_router_mutex;
static MessageRouter::ptr g_router;

static void InstallObserver(const MessageRouter::ptr& router)
{
    std::weak_ptr<MessageRouter> weak = router;
    auto registry = WSRegistry::GetInstance();
    registry->setConnectionObserver(
        [weak](bool added, int64_t user_id, const std::string& device_id, int64_t old_user_id) {
            if (auto current = weak.lock()) {
                current->OnConnection(added, user_id, device_id, old_user_id);
            }
        });
    registry->setPresenceCallback([weak]() {
        if (auto current = weak.lock()) {
            current->BroadcastPresence();
        }
    });
}

MessageRouter::ptr MessageRouter::Get()
{
    FiberMutex::Lock lock(g_router_mutex);
    if (!g_router) {
        g_router = std::make_shared<MessageRouterRedis>();
        g_router->Start();
        InstallObserver(g_router);
    }
    return g_router;
}

void MessageRouter::Init()
{
    auto router = Get();
    if (router) {
        router->Start();
    }
}

void MessageRouter::Shutdown()
{
    FiberMutex::Lock lock(g_router_mutex);
    auto registry = WSRegistry::GetInstance();
    registry->setConnectionObserver(nullptr);
    registry->setPresenceCallback(nullptr);
    if (g_router) {
        g_router->Stop();
        g_router.reset();
    }
}

int32_t SendNotificationToUser(int64_t user_id, const std::string& message)
{
    return MessageRouter::Get()->SendToUser(user_id, BuildChannelMessage(message, "notification"));
}

void BroadcastNotification(const std::string& message)
{
    MessageRouter::Get()->Broadcast(BuildChannelMessage(message, "notification"));
}

int32_t SendChatToUser(int64_t user_id, const std::string& message)
{
    return MessageRouter::Get()->SendToUser(user_id, BuildChannelMessage(message, "chat"));
}
