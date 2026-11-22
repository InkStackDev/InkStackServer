/**
 * @file NotificationRepository.hpp
 * @brief 通知单表读写与对象缓存
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

class NotificationRepository {
public:
    static DB::NotificationInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool Insert(DB::NotificationInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::NotificationInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::NotificationInfo::ptr& info);

    static void Evict(int64_t id);

    static bool ListUnreadByUser(std::vector<DB::NotificationInfo::ptr>& results, int64_t user_id, chen::IDB::ptr conn);

    static bool ListUnreadByUserType(std::vector<DB::NotificationInfo::ptr>& results,
        int64_t user_id,
        const std::string& type,
        chen::IDB::ptr conn);

    /**
     * @brief 将已过期的广播标记清除（物理更新）
     */
    static bool ClearExpiredBroadcasts(int64_t now, chen::IDB::ptr conn);
};
