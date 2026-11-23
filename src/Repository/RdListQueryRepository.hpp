/**
 * @file RdListQueryRepository.hpp
 * @brief 研发列表查询（数据范围 + 过滤 + 分页）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include "DB/BugInfo.hpp"
#include "DB/RequirementInfo.hpp"
#include "DB/TaskInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief 研发实体列表查询
 * @details 数据范围在 SQL 中应用：
 *          - 需求/缺陷：只看 role >= REPORTER 的组织
 *          - 任务：role >= REPORTER 看全部；普通成员只看自己创建/负责的任务
 *          空范围返回 0 行，不会退化成全组织。
 */
class RdListQueryRepository {
public:
    struct Filter {
        int32_t status = -1;
        int32_t priority = -1;
        int32_t severity = -1;
        int64_t assignee_id = 0;
        std::string search;
        uint64_t offset = 0;
        uint64_t limit = 0;
    };

    static bool ListTasks(std::vector<DB::TaskInfo::ptr>& out,
        int64_t& total,
        const Viewer& viewer,
        const OrgScope& scope,
        int64_t org_id,
        const Filter& filter,
        chen::IDB::ptr conn);

    static bool ListRequirements(std::vector<DB::RequirementInfo::ptr>& out,
        int64_t& total,
        const OrgScope& scope,
        int64_t org_id,
        const Filter& filter,
        chen::IDB::ptr conn);

    static bool ListBugs(std::vector<DB::BugInfo::ptr>& out,
        int64_t& total,
        const OrgScope& scope,
        int64_t org_id,
        const Filter& filter,
        chen::IDB::ptr conn);
};
