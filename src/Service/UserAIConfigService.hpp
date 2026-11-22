/**
 * @file UserAIConfigService.hpp
 * @brief 用户 AI 配置用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserAiConfigInfo.hpp"

class UserAIConfigService {
public:
    /**
     * @brief 保存配置并刷新缓存（按 user_id 回查 DB 生成的主键）
     */
    static bool Save(DB::UserAiConfigInfo::ptr info);
};
