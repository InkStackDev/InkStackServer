/**
 * @file NotificationService.hpp
 * @brief 通知用例：创建、已读、广播关闭与过期清理
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/NotificationInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct NotificationData {
    int64_t user_id = 0;
    std::string title;
    std::string content;
    std::string type;
    int64_t sender_id = 0;
    int64_t article_id = 0;
    int64_t comment_id = 0;
    int32_t is_broadcast = 0;
    std::string level = "";
    int64_t start_time = 0;
    int64_t end_time = 0;
};

class NotificationService {
public:
    /**
     * @brief 创建通知并失效用户通知缓存
     */
    static DB::NotificationInfo::ptr AddNotification(const NotificationData& data);

    /**
     * @brief 标记单条通知已读（校验归属）
     */
    static bool MarkRead(int64_t uid, int64_t notification_id);

    /**
     * @brief 批量标记已读，只处理属于 uid 的通知
     * @return 实际更新条数
     */
    static int64_t MarkRead(int64_t uid, const std::vector<int64_t>& ids);

    /**
     * @brief 标记用户全部未读通知为已读
     */
    static int64_t MarkAllRead(int64_t uid);

    /**
     * @brief 标记用户某类型的未读通知为已读
     */
    static int64_t MarkReadByType(int64_t uid, const std::string& type);

    /**
     * @brief 关闭广播（is_broadcast = 0）
     */
    static bool CloseBroadcast(int64_t notification_id);

    /**
     * @brief 清理已过期的广播标记
     * @return 是否执行成功
     */
    static bool CleanupExpiredBroadcasts();

    static bool ListByUser(std::vector<DB::NotificationInfo::ptr>& out,
        int64_t uid,
        int32_t offset,
        int32_t size,
        const std::string& type);

    static int64_t CountByUser(int64_t uid, const std::string& type);

    static int64_t UnreadCount(int64_t uid);

    static bool ListBroadcasts(std::vector<DB::NotificationInfo::ptr>& out, int64_t offset, int64_t size);
};
