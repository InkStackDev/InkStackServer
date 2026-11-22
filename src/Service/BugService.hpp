/**
 * @file BugService.hpp
 * @brief 缺陷用例：保存与批量删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include "DB/BugInfo.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

struct BugInput {
    int64_t id = 0;
    int64_t org_id = 0;
    int64_t creator_id = 0;
    std::string title;
    std::string description;
    /// -1 表示不修改
    int32_t severity = -1;
    /// -1 表示不修改
    int32_t priority = -1;
    /// -1 表示不修改
    int32_t status = -1;
    int64_t assignee_id = 0;
    int64_t requirement_id = 0;
    std::string module;
    std::string steps_to_reproduce;
    std::string environment;
};

class BugService {
public:
    enum class SaveResult {
        Ok,
        NotFound,
        NotMember,
        AccessDenied,
        DbError
    };

    enum class DeleteResult {
        Ok,
        AccessDenied,
        DbError
    };

    enum class ViewResult {
        Ok,
        NotFound,
        AccessDenied,
        DbError
    };

    static ViewResult GetDetail(const Viewer& viewer, int64_t id, DB::BugInfo::ptr& out);

    static SaveResult Save(const Viewer& viewer, const BugInput& input, DB::BugInfo::ptr& out);

    static DeleteResult DeleteMany(const Viewer& viewer,
        int64_t org_id,
        const std::set<int64_t>& ids,
        std::vector<DB::BugInfo::ptr>& deleted);
};
