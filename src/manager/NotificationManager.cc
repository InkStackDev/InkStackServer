#include "NotificationManager.hpp"

#include "../Message/MessageRouter.hpp"
#include "../Util/Util.hpp"
#include "CacheUtil.hpp"
#include "WSConnectionManager.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 120;
static const char* kCachePrefix = "cache:obj:notif:";

static std::string NotificationCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::NotificationInfo::ptr ParseNotificationCache(const Json::Value& v)
{
    auto info = std::make_shared<data::NotificationInfo>();
    info->setId(CacheJsonInt64(v, "id"));
    info->setUserId(CacheJsonInt64(v, "user_id"));
    info->setTitle(CacheJsonString(v, "title"));
    info->setContent(CacheJsonString(v, "content"));
    info->setType(CacheJsonString(v, "type"));
    info->setSenderId(CacheJsonInt64(v, "sender_id"));
    info->setArticleId(CacheJsonInt64(v, "article_id"));
    info->setCommentId(CacheJsonInt64(v, "comment_id"));
    info->setIsRead(CacheJsonInt32(v, "is_read"));
    info->setReadTime(CacheJsonTime(v, "read_time"));
    info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(CacheJsonTime(v, "create_time"));
    info->setUpdateTime(CacheJsonTime(v, "update_time"));
    info->setIsBroadcast(CacheJsonInt32(v, "is_broadcast"));
    info->setLevel(CacheJsonString(v, "level"));
    info->setStartTime(CacheJsonInt64(v, "start_time"));
    info->setEndTime(CacheJsonInt64(v, "end_time"));
    info->markClean();
    return info;
}

static data::NotificationInfo::ptr GetNotificationCache(int64_t id)
{
    std::string key = NotificationCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseNotificationCache(v);
}

static void SetNotificationCache(const data::NotificationInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(NotificationCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

data::NotificationInfo::ptr NotificationManager::ParseRow(chen::ISQLData::ptr rt)
{
    return data::NotificationInfoDao::ParseRow(rt);
}

// ========== WS推送 ==========

int32_t NotificationManager::sendToUser(int64_t user_id, const std::string& message)
{
    return MessageRouter::Get()->SendToUser(user_id, BuildChannelMessage(message, "notification"));
}

void NotificationManager::broadcast(const std::string& message)
{
    MessageRouter::Get()->Broadcast(BuildChannelMessage(message, "notification"));
}

int32_t NotificationManager::getOnlineCount()
{
    return static_cast<int32_t>(MessageRouter::Get()->OnlineUserCount());
}

// ========== DB persistence ==========

data::NotificationInfo::ptr NotificationManager::addNotification(const NotificationData& data)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::NotificationInfo>();
    info->setUserId(data.user_id);
    info->setTitle(data.title);
    info->setContent(data.content);
    info->setType(data.type);
    info->setSenderId(data.sender_id);
    info->setArticleId(data.article_id);
    info->setCommentId(data.comment_id);
    info->setIsRead(0);
    info->setReadTime(0);
    info->setIsDeleted(0);
    info->setIsBroadcast(data.is_broadcast);
    info->setLevel(data.level);
    info->setStartTime(data.start_time);
    info->setEndTime(data.end_time);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::NotificationInfoDao::Insert(info, db)) {
        ERROR(logger) << "NotificationManager addNotification Insert fail";
        return nullptr;
    }

    SetNotificationCache(info);
    return info;
}

void NotificationManager::update(data::NotificationInfo::ptr info)
{
    SetNotificationCache(info);
}

void NotificationManager::listByUser(
    std::vector<data::NotificationInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size,
    const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);

    // 结果缓存：仅对首页做缓存
    std::string typeKey = type.empty() ? "all" : type;
    std::string listKey = "notif:list:" + std::to_string(user_id) + ":" + typeKey + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (GetCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                results.push_back(info);
            }
        }
        return;
    }

    if (data::NotificationInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    std::vector<int64_t> ids;
    for (auto& info : results) {
        ids.push_back(info->getId());
    }
    CacheListResult(listKey, ids);
}

int64_t NotificationManager::countByUser(int64_t user_id, const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    std::string ck = "notif:cnt:" + std::to_string(user_id) + ":" + (type.empty() ? "all" : type);
    return ExecuteCountCached(qb, db, ck);
}

int64_t NotificationManager::unreadCount(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    return ExecuteCountCached(qb, db, "notif:unread:" + std::to_string(user_id));
}

bool NotificationManager::markRead(int64_t notification_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto info = get(notification_id);
    if (!info) {
        return false;
    }

    info->setIsRead(1);
    info->setReadTime(time(0));
    if (data::NotificationInfoDao::Update(info, db)) {
        return false;
    }
    SetNotificationCache(info);
    return true;
}

bool NotificationManager::markRead(const std::vector<int64_t>& ids)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    for (auto id : ids) {
        auto info = get(id);
        if (!info) {
            continue;
        }
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            SetNotificationCache(info);
        }
    }
    return true;
}

int64_t NotificationManager::markAllRead(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    // Find all unread, non-deleted notifications for this user
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<data::NotificationInfo::ptr> unread;
    if (data::NotificationInfoDao::QueryByBuilder(unread, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }

    int64_t count = 0;
    for (auto& info : unread) {
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            SetNotificationCache(info);
            count++;
        }
    }
    return count;
}

int64_t NotificationManager::markReadByType(int64_t user_id, const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("type", "=", type);
    std::vector<data::NotificationInfo::ptr> unread;
    if (data::NotificationInfoDao::QueryByBuilder(unread, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }

    int64_t count = 0;
    for (auto& info : unread) {
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            SetNotificationCache(info);
            count++;
        }
    }
    return count;
}

data::NotificationInfo::ptr NotificationManager::get(int64_t id)
{
    auto v = GetNotificationCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::NotificationInfoDao::Query(id, db);
    if (v) {
        SetNotificationCache(v);
    }
    return v;
}

void NotificationManager::cleanupExpiredBroadcasts()
{
    auto db = GetDB();
    if (!db) {
        return;
    }
    int64_t now = time(0);
    // 将已过期的广播的 is_broadcast 置 0，前端 /broadcast/list 不再返回
    auto qb = data::NotificationInfoDao::newQuery();
    qb->set("is_broadcast", (int64_t)0);
    qb->set("update_time", now);
    qb->where("is_broadcast", "=", (int64_t)1);
    qb->whereSQL("end_time > ?", (int64_t)0);
    qb->where("end_time", "<=", now);
    auto stmt = db->prepare(qb->buildUpdateSQL());
    if (!stmt) {
        return;
    }
    qb->bindUpdateParams(stmt);
    stmt->execute();
}
