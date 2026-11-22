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

    /**
     * @brief 当前用户可进入研发平台的组织（role >= REPORTER）
     */
    static bool ListUserOrganizations(const Viewer& viewer, std::vector<UserOrganizationView>& out);

    /**
     * @brief 组织成员列表；普通成员可查看，非成员拒绝
     */
    static ListResult ListMembers(const Viewer& viewer, int64_t org_id, std::vector<OrganizationMemberView>& out);

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
