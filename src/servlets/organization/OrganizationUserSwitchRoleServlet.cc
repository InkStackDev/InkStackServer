#include "OrganizationUserSwitchRoleServlet.hpp"

#include <json/json.h>

#include "../../Event/EventDefine.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/UserRepository.hpp"

OrganizationUserSwitchRoleServlet::OrganizationUserSwitchRoleServlet()
    : BlogLoginedServlet("OrganizationUserSwitchRoleServlet")
{
}

int32_t OrganizationUserSwitchRoleServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");           // organization_user_rel id
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");   // organization id
        DEFINE_AND_CHECK_TYPE(result, int32_t, role, "role");       // new role
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id"); // user_id

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        if (role != OrgRole::MEMBER && role != OrgRole::REPORTER && role != OrgRole::DEVELOPER
            && role != OrgRole::DEV_LEAD && role != OrgRole::ORG_ADMIN) {
            result->setErrno(PARAM_INVALID, "invalid role");
            break;
        }

        DB::OrganizationUserRelInfo::ptr rel;
        auto rt = OrganizationService::SwitchRole(viewer, id, org_id, user_id, role, rel);
        if (rt == OrganizationService::MemberResult::NotMember) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        if (rt == OrganizationService::MemberResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationService::MemberResult::RelNotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }
        if (rt != OrganizationService::MemberResult::Ok) {
            result->setErrno(ORG_USER_REL_FAILED);
            break;
        }

        // Notify user about role change
        {
            auto org = OrganizationRepository::Get(org_id, GetDB());
            std::string org_name = org ? org->getName() : std::to_string(org_id);
            const char* role_name = "普通成员";
            if (role == OrgRole::REPORTER) {
                role_name = "报告者";
            } else if (role == OrgRole::DEVELOPER) {
                role_name = "开发者";
            } else if (role == OrgRole::DEV_LEAD) {
                role_name = "研发主管";
            } else if (role == OrgRole::ORG_ADMIN) {
                role_name = "组织管理员";
            }

            EventOrgMemberData data = {};
            data.type = "role_change";
            data.org_id = org_id;
            data.org_name = org_name;
            data.operator_id = viewer.user_id;
            data.target_user_id = user_id;
            data.new_role_name = role_name;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_MEMBER, std::move(data));
        }

        auto user = UserRepository::Get(rel->getUserId(), GetDB());
        result->set("id", rel->getId());
        result->set("user_id", rel->getUserId());
        result->set("name", user ? user->getName() : "");
        result->set("avatar", user ? user->getAvatar() : "");
        result->set("email", user ? user->getEmail() : "");
        result->set("role", rel->getRole());
        result->set("status", rel->getStatus());
        result->set("join_time", rel->getCreateTime());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
