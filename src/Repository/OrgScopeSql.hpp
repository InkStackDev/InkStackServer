/**
 * @file OrgScopeSql.hpp
 * @brief 将组织数据范围应用到 QueryBuilder
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include <Chen/DB/QueryBuilder.hpp>

#include <cstdint>
#include <string>

/**
 * @brief 为查询追加组织数据范围条件
 * @param qb 查询构造器
 * @param column 组织 id 列（可带表别名）
 * @param scope 数据范围
 * @param min_role 可见所需的最低组织角色
 * @note 空 scope 会追加恒假条件，绝不能退化成“查全部组织”
 */
void ApplyOrgScope(const chen::QueryBuilder::ptr& qb,
    const std::string& column,
    const OrgScope& scope,
    int32_t min_role);
