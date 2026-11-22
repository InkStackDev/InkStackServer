#pragma once

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "DB/NotificationInfo.hpp"

#include "../Service/NotificationService.hpp"

class NotificationManager {
public:
    // WS 推送（连接统一由 WSMgr 管理）
    int32_t sendToUser(int64_t user_id, const std::string& message);

    void broadcast(const std::string& message);

    int32_t getOnlineCount();

    // DB persistence
    DB::NotificationInfo::ptr get(int64_t id);

    DB::NotificationInfo::ptr addNotification(const NotificationData& data);

    void update(DB::NotificationInfo::ptr info);

    void listByUser(std::vector<DB::NotificationInfo::ptr>& results,
        int64_t user_id,
        uint64_t offset,
        uint64_t size,
        const std::string& type = "");

    int64_t countByUser(int64_t user_id, const std::string& type = "");

    int64_t unreadCount(int64_t user_id);

    bool markRead(int64_t uid, int64_t notification_id);

    int64_t markRead(int64_t uid, const std::vector<int64_t>& ids);

    int64_t markAllRead(int64_t uid);

    int64_t markReadByType(int64_t uid, const std::string& type);

    bool cleanupExpiredBroadcasts();
};

typedef chen::Singleton<NotificationManager> NotificationMgr;
