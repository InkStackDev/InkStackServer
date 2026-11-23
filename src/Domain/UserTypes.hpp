/**
 * @file UserTypes.hpp
 * @brief 用户领域枚举（角色 / 状态）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

struct UserRole {
    enum : int32_t {
        USER = 1,
        ADMIN = 2,
        EDITOR = 3,
        CHECKER = 4
    };
};

struct UserState {
    enum : int32_t {
        INACTIVE = 0,
        ACTIVE = 1,
        BANNED = 2
    };
};
