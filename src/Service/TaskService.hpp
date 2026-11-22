/**
 * @file TaskService.hpp
 * @brief 任务用例：保存与批量删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/TaskInfo.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct TaskInput {
    int64_t id = 0;
    int64_t org_id = 0;
    int64_t creator_id = 0;
    std::string title;
    std::string description;
    /// -1 表示不修改
    int32_t priority = -1;
    /// -1 表示不修改
    int32_t status = -1;
    int64_t assignee_id = 0;
    int64_t requirement_id = 0;
    int64_t bug_id = 0;
    int64_t deadline = 0;
    int64_t estimated_hours = 0;
};

class TaskService {
public:
    enum class SaveResult {
        Ok,
        NotFound,
        DbError
    };

    static SaveResult Save(const TaskInput& input, DB::TaskInfo::ptr& out);

    static bool DeleteMany(int64_t org_id,
        const std::set<int64_t>& ids,
        int64_t uid,
        int32_t org_role,
        bool is_platform_admin,
        std::vector<DB::TaskInfo::ptr>& deleted);
};
