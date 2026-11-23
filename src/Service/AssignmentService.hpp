/**
 * @file AssignmentService.hpp
 * @brief 作业用例：创建/更新、组织布置、批量删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/AssignmentInfo.hpp"
#include "DB/AssignmentOrganizationRelInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct AssignmentInput {
    /// 0 表示新建；非 0 表示按 ID 更新
    int64_t id = 0;
    std::string name;
    std::string subject_name;
    std::string description;
    std::string file_type;
    int32_t priority = 0;
    int32_t status = 0;
    int32_t max_size = 0;
    int64_t deadline = 0;
};

class AssignmentService {
public:
    enum class CreateResult {
        Ok,
        NotFound,
        DbError
    };

    enum class OrgCreateResult {
        Ok,
        NotFound,
        DbError
    };

    enum class ViewResult {
        Ok,
        NotFound,
        DbError
    };

    struct DetailView {
        int64_t id = 0;
        std::string name;
        std::string subject_name;
        std::string description;
        std::string file_type;
        int32_t priority = 0;
        int32_t status = 0;
        int32_t max_size = 0;
        int64_t deadline = 0;
        int64_t create_time = 0;
        int64_t update_time = 0;
    };

    struct ResourceView {
        int64_t id = 0;
        std::string name;
        std::string path;
        int64_t size = 0;
    };

    struct SubmissionView {
        bool has_submission = false;
        int64_t user_id = 0;
        std::string user_name;
        std::string user_avatar;
        int64_t submit_time = 0;
        int32_t status = 0;
        std::vector<ResourceView> resources;
    };

    static ViewResult GetDetail(int64_t id, DetailView& out);

    static ViewResult GetSubmission(int64_t assignment_id, int64_t uid, SubmissionView& out);

    static void ListSubmitters(int64_t assignment_id, std::vector<int64_t>& out);

    /**
     * @brief 管理端创建/更新作业（按 subject_name + name 复用记录）
     */
    static CreateResult CreateOrUpdate(const AssignmentInput& input, DB::AssignmentInfo::ptr& out);

    /**
     * @brief 组织内布置作业：创建/更新作业，并在新建时写入组织关联
     * @param[out] created 是否新建了作业
     */
    static OrgCreateResult CreateForOrganization(int64_t org_id,
        const std::string& assigned_by,
        const AssignmentInput& input,
        DB::AssignmentInfo::ptr& out,
        bool& created);

    /**
     * @brief 批量软删除作业
     * @param[out] deleted 实际删除的作业，供调用方发送事件
     */
    static bool DeleteMany(const std::vector<int64_t>& ids, std::vector<DB::AssignmentInfo::ptr>& deleted);
};
