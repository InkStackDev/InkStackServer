/**
 * @file OrganizationService.hpp
 * @brief 组织用例：创建/删除、加入/审核、角色与申请
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include "DB/OrganizationApplyInfo.hpp"
#include "DB/OrganizationInfo.hpp"
#include "DB/OrganizationUserRelInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct OrganizationInput {
    int64_t id = 0;
    std::string name;
    std::string type;
    std::string description;
    int32_t status = 0;
    int64_t owner_id = 0;
};

class OrganizationService {
public:
    enum class Result {
        Ok,
        NotFound,
        Conflict,
        AccessDenied,
        DbError
    };

    enum class DeleteResult {
        Ok,
        AccessDenied,
        NoValidOrgs,
        DbError
    };

    enum class JoinResult {
        Ok,
        OrgNotFound,
        OrgDisabled,
        AlreadyJoined,
        AlreadyApplied,
        DbError
    };

    enum class MemberResult {
        Ok,
        RelNotFound,
        InvalidState,
        NotMember,
        AccessDenied,
        DbError
    };

    enum class ListResult {
        Ok,
        NotMember,
        AccessDenied,
        DbError
    };

    struct UserOrganizationView {
        int64_t org_id = 0;
        std::string name;
        int32_t role = 0;
    };

    struct OrganizationMemberView {
        int64_t user_id = 0;
        std::string name;
        std::string avatar;
        int32_t role = 0;
    };

    struct OrganizationMemberRow {
        int64_t id = 0;
        int64_t user_id = 0;
        std::string name;
        std::string avatar;
        std::string email;
        int32_t role = 0;
        int32_t status = 0;
        int64_t join_time = 0;
    };

    struct OrganizationDetailView {
        int64_t id = 0;
        std::string name;
        std::string description;
        std::string type;
        int32_t status = 0;
        bool has_membership = false;
        int32_t member_status = 0;
        int32_t member_role = 0;
    };

    struct OrganizationListItem {
        int64_t id = 0;
        std::string name;
        std::string type;
        std::string description;
        int32_t status = 0;
        int64_t member_count = 0;
    };

    struct OrganizationStatsView {
        int64_t total_members = 0;
        int64_t active_members = 0;
        int64_t org_admin_count = 0;
        int64_t regular_count = 0;
    };

    struct UserBrief {
        std::string name;
        std::string avatar;
        std::string email;
    };

    struct OrganizationApplyView {
        int64_t id = 0;
        int64_t user_id = 0;
        std::string user_name;
        std::string org_name;
        std::string org_type;
        std::string org_description;
        int32_t status = 0;
        std::string review_reason;
        int64_t created_at = 0;
        int64_t reviewed_at = 0;
    };

    struct OrganizationAssignmentView {
        int64_t id = 0;
        int64_t assign_id = 0;
        std::string assigned_by;
        std::string name;
        std::string subject_name;
        int64_t end_time = 0;
        int64_t max_size = 0;
        int32_t status = 0;
        int32_t priority = 0;
        std::string file_type;
        std::string description;
    };

    /**
     * @brief 当前用户可进入研发平台的组织（role >= REPORTER）
     */
    static bool ListUserOrganizations(const Viewer& viewer, std::vector<UserOrganizationView>& out);

    /**
     * @brief 组织成员列表；普通成员可查看，非成员拒绝
     */
    static ListResult ListMembers(const Viewer& viewer, int64_t org_id, std::vector<OrganizationMemberView>& out);

    /**
     * @brief 组织详情 + 当前用户成员状态
     */
    static Result GetDetail(const Viewer& viewer, int64_t org_id, OrganizationDetailView& out);

    /**
     * @brief 按状态分页查询组织成员
     * @details PENDING 列表要求成员身份 + 管理权限；其他状态保持原行为
     */
    static ListResult ListMemberPage(const Viewer& viewer,
        int64_t org_id,
        int32_t status,
        uint64_t offset,
        uint64_t limit,
        std::vector<OrganizationMemberRow>& out,
        int64_t& total);

    /**
     * @brief 组织已布置作业列表（管理员或 DEV_LEAD 及以上）
     */
    static ListResult ListAssignments(const Viewer& viewer,
        int64_t org_id,
        int32_t status,
        uint64_t offset,
        uint64_t limit,
        std::vector<OrganizationAssignmentView>& out,
        int64_t& total);

    static bool ListOrganizations(int32_t status,
        uint64_t offset,
        uint64_t limit,
        std::vector<OrganizationListItem>& out,
        int64_t& total);

    static Result GetStats(int64_t org_id, OrganizationStatsView& out);

    static std::string GetOrgName(int64_t org_id);

    static std::string GetUserName(int64_t user_id);

    static bool GetUserProfile(int64_t user_id, UserBrief& out);

    static ListResult ListApplies(const Viewer& viewer,
        int32_t status,
        uint64_t offset,
        uint64_t limit,
        std::vector<OrganizationApplyView>& out,
        int64_t& total);

    static ListResult ListMyApplies(int64_t user_id,
        uint64_t offset,
        uint64_t limit,
        std::vector<OrganizationApplyView>& out,
        int64_t& total);

    static Result CreateOrUpdate(const Viewer& viewer,
        const OrganizationInput& input,
        DB::OrganizationInfo::ptr& out,
        bool& created);

    /**
     * @brief 批量软删除组织及其成员关系
     */
    static DeleteResult DeleteMany(const Viewer& viewer,
        const std::vector<int64_t>& ids,
        std::vector<DB::OrganizationInfo::ptr>& orgs,
        std::vector<DB::OrganizationUserRelInfo::ptr>& rels);

    static Result SubmitApply(int64_t uid,
        const std::string& name,
        const std::string& type,
        const std::string& description,
        DB::OrganizationApplyInfo::ptr& out);

    /**
     * @brief 审核组织申请；通过时创建组织并把申请人设为组织管理员
     */
    static Result ReviewApply(const Viewer& viewer,
        int64_t apply_id,
        bool approve,
        const std::string& reason,
        DB::OrganizationInfo::ptr& org_out,
        DB::OrganizationApplyInfo::ptr& apply_out);

    static JoinResult Join(int64_t org_id, int64_t uid, DB::OrganizationUserRelInfo::ptr& out);

    /**
     * @brief 审核成员加入申请（仅 PENDING 可变更）
     */
    static MemberResult SetMemberState(const Viewer& viewer,
        int64_t org_id,
        int64_t uid,
        int32_t state,
        DB::OrganizationUserRelInfo::ptr& out);

    static MemberResult Kick(const Viewer& viewer, int64_t org_id, int64_t uid, DB::OrganizationUserRelInfo::ptr& out);

    static MemberResult SwitchRole(const Viewer& viewer,
        int64_t rel_id,
        int64_t org_id,
        int64_t uid,
        int32_t role,
        DB::OrganizationUserRelInfo::ptr& out);
};
