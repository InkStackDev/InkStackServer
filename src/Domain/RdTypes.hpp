/**
 * @file RdTypes.hpp
 * @brief RD 领域枚举（任务 / 缺陷 / 需求 / 作业状态）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

struct TaskStatus {
    enum : int32_t {
        TODO = 0,
        IN_PROGRESS = 1,
        DONE = 2,
        CLOSED = 3
    };
};

struct BugStatus {
    enum : int32_t {
        PENDING = 0,
        IN_PROGRESS = 1,
        FIXED = 2,
        CLOSED = 3,
        REOPENED = 4
    };
};

struct RequirementStatus {
    enum : int32_t {
        DRAFT = 0,
        PENDING = 1,
        IN_PROGRESS = 2,
        DONE = 3,
        CLOSED = 4
    };
};

struct AssignmentStatus {
    enum : int32_t {
        DRAFT = 0,
        ACTIVE = 1,
        INACTIVE = 2
    };
};
