/**
 * @file CommentTypes.hpp
 * @brief 评论领域枚举（审核状态）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

struct CommentStatus {
    enum : int32_t {
        PENDING = 1,
        APPROVED = 2,
        REJECTED = 3,
        SPAM = 4
    };
};
