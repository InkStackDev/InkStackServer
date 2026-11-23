/**
 * @file UserAIConfigRepository.hpp
 * @brief 用户 AI 配置读写与缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserAiConfigInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>

class UserAIConfigRepository {
public:
    static DB::UserAiConfigInfo::ptr GetByUserId(int64_t user_id, chen::IDB::ptr conn);

    /**
     * @brief 绕过缓存直接回查并刷新（保存后取 DB 生成的主键）
     */
    static DB::UserAiConfigInfo::ptr ReloadByUserId(int64_t user_id, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::UserAiConfigInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::UserAiConfigInfo::ptr& info);

    static void Evict(int64_t id);
};
