#include "NotificationManager.hpp"

#include "../Message/MessageRouter.hpp"
#include "../Repository/NotificationQueryRepository.hpp"
#include "../Repository/NotificationRepository.hpp"
#include "../Util/Util.hpp"
#include "WSConnectionManager.hpp"

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

DB::NotificationInfo::ptr NotificationManager::addNotification(const NotificationData& data)
{
    return NotificationService::AddNotification(data);
}

void NotificationManager::update(DB::NotificationInfo::ptr info)
{
    NotificationRepository::Put(info);
}

void NotificationManager::listByUser(std::vector<DB::NotificationInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size,
    const std::string& type)
{
    NotificationQueryRepository::ListByUser(results, user_id, offset, size, type, GetDB());
}

int64_t NotificationManager::countByUser(int64_t user_id, const std::string& type)
{
    return NotificationQueryRepository::CountByUser(user_id, type, GetDB());
}

int64_t NotificationManager::unreadCount(int64_t user_id)
{
    return NotificationQueryRepository::UnreadCount(user_id, GetDB());
}

bool NotificationManager::markRead(int64_t uid, int64_t notification_id)
{
    return NotificationService::MarkRead(uid, notification_id);
}

int64_t NotificationManager::markRead(int64_t uid, const std::vector<int64_t>& ids)
{
    return NotificationService::MarkRead(uid, ids);
}

int64_t NotificationManager::markAllRead(int64_t uid)
{
    return NotificationService::MarkAllRead(uid);
}

int64_t NotificationManager::markReadByType(int64_t uid, const std::string& type)
{
    return NotificationService::MarkReadByType(uid, type);
}

DB::NotificationInfo::ptr NotificationManager::get(int64_t id)
{
    return NotificationRepository::Get(id, GetDB());
}

bool NotificationManager::cleanupExpiredBroadcasts()
{
    return NotificationService::CleanupExpiredBroadcasts();
}
