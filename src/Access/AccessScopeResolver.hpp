/**
 * @file AccessScopeResolver.hpp
 * @brief 将访问者身份解析为组织数据范围
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include <Chen/DB/DB.hpp>

/**
 * @brief 组织数据范围解析器
 * @details 一次查询拿到用户所有已批准、未删除、启用组织的角色；
 *          平台管理员返回 All()；查询失败返回 false，由调用方映射为 DB 错误。
 */
class AccessScopeResolver {
public:
    static bool ResolveOrgs(OrgScope& scope, const Viewer& viewer, chen::IDB::ptr conn);
};
