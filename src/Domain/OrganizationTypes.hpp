/**
 * @file OrganizationTypes.hpp
 * @brief 组织领域枚举（角色 / 状态 / 成员状态）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

struct OrgRole {
    enum : int32_t {
        MEMBER = 1,
        REPORTER = 2,
        DEVELOPER = 3,
        DEV_LEAD = 4,
        ORG_ADMIN = 5
    };
};

struct OrgStatus {
    enum : int32_t {
        INACTIVE = 0,
        ACTIVE = 1
    };
};

struct OrgMemberStatus {
    enum : int32_t {
        PENDING = 0,
        APPROVED = 1,
        REJECTED = 2,
        EXITED = 3
    };
};
