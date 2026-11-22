/**
 * @file ConversationRepository.hpp
 * @brief 私信会话单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ConversationInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class ConversationRepository {
public:
    static DB::ConversationInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::ConversationInfo::ptr GetByUsers(int64_t uid_a, int64_t uid_b, chen::IDB::ptr conn);

    static bool Insert(DB::ConversationInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::ConversationInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::ConversationInfo::ptr& info);

    static void Evict(int64_t id);

    /// 参与者归一化：user_a_id 恒为较小值
    static int64_t NormalizeUserA(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_a : uid_b; }

    static int64_t NormalizeUserB(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_b : uid_a; }
};
