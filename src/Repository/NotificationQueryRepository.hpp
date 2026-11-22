/**
 * @file NotificationQueryRepository.hpp
 * @brief 通知列表/计数/广播查询
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/NotificationInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class NotificationQueryRepository {
public:
    static void ListByUser(std::vector<DB::NotificationInfo::ptr>& results,
        int64_t user_id,
        uint64_t offset,
        uint64_t size,
        const std::string& type,
        chen::IDB::ptr conn);

    static int64_t CountByUser(int64_t user_id, const std::string& type, chen::IDB::ptr conn);

    static int64_t UnreadCount(int64_t user_id, chen::IDB::ptr conn);

    static void ListBroadcasts(std::vector<DB::NotificationInfo::ptr>& results,
        uint64_t offset,
        uint64_t size,
        chen::IDB::ptr conn);

    /**
     * @brief 用户通知列表/计数/未读数变化后失效其缓存
     */
    static void InvalidateUser(int64_t user_id);
};
