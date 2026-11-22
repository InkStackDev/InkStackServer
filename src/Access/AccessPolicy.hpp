/**
 * @file AccessPolicy.hpp
 * @brief 单实体访问策略（纯函数，不依赖 HTTP / errcode）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include <cstdint>

/**
 * @brief 命令与单实体读取的权限策略
 * @details 只回答“能不能看/能不能操作”，错误码映射留在入口层。
 */
class AccessPolicy {
public:
    static bool CanViewRequirement(const Viewer& viewer, const OrgScope& scope, int64_t org_id);

    static bool CanViewBug(const Viewer& viewer, const OrgScope& scope, int64_t org_id);

    static bool CanViewTask(const Viewer& viewer,
        const OrgScope& scope,
        int64_t org_id,
        int64_t creator_id,
        int64_t assignee_id);

    static bool CanCreateRdItem(const Viewer& viewer, const OrgScope& scope, int64_t org_id);

    static bool CanEditRdItem(const Viewer& viewer, const OrgScope& scope, int64_t org_id);

    static bool CanDeleteRdItem(const Viewer& viewer, const OrgScope& scope, int64_t org_id, int64_t creator_id);
};
