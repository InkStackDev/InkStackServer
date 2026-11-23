/**
 * @file AuthContext.hpp
 * @brief 请求级认证上下文
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

#include "DB/UserInfo.hpp"

/**
 * @brief 一次请求解析出的登录上下文
 */
struct AuthContext {
    int64_t uid = 0;
    std::string session_id;
    DB::UserInfo::ptr user;

    bool valid() const { return uid > 0 && user != nullptr; }
};
