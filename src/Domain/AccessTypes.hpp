/**
 * @file AccessTypes.hpp
 * @brief 数据访问范围类型（Viewer / OrgScope）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <vector>

/**
 * @brief 当前请求的访问者身份
 * @note 只包含认证与平台级角色，不包含任何业务数据范围
 */
struct Viewer {
    int64_t user_id = 0;
    int32_t system_role = 0;
    bool is_platform_admin = false;

    bool valid() const;
};

/**
 * @brief 用户在一个组织内的授权
 */
struct OrgGrant {
    int64_t org_id = 0;
    int32_t role = 0;
};

/**
 * @brief 组织级数据范围
 * @details 查询型 Repository 只消费该类型，不再接收调用方算好的 org_ids / is_admin。
 *          - All()：平台管理员或系统任务，表示不限制组织
 *          - ForGrants()：普通用户，空集合表示没有任何组织权限（必须查不到数据）
 */
class OrgScope {
public:
    static OrgScope ForGrants(std::vector<OrgGrant> grants);

    bool isAll() const;

    bool empty() const;

    bool allows(int64_t org_id, int32_t min_role) const;

    std::vector<int64_t> orgIds(int32_t min_role) const;

private:
    friend class AccessScopeResolver;

    static OrgScope All();

    bool all_ = false;
    std::vector<OrgGrant> grants_;
};
