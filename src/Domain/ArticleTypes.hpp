/**
 * @file ArticleTypes.hpp
 * @brief 文章领域枚举（状态 / 类型）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

struct ArticleStatus {
    enum : int32_t {
        UNKNOWN = 0,
        CHECKING = 1,
        PUBLISHED = 2,
        REJECTED = 3,
        PRIVATE = 4
    };
};

struct ArticleType {
    enum : int32_t {
        ORIGINAL = 1,
        REPRINT = 2
    };
};
