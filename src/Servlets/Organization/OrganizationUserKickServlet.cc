#include "OrganizationUserKickServlet.hpp"

#include <json/json.h>

#include "../../Event/EventDefine.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationUserKickServlet::OrganizationUserKickServlet()
    : BlogLoginedServlet("OrganizationUserKickServlet")
{
}

int32_t OrganizationUserKickServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");           // organization_user_rel id
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");   // organization id
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id"); // user_id

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::OrganizationUserRelInfo::ptr rel;
        auto rt = OrganizationService::Kick(viewer, org_id, user_id, rel);
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

        // Notify org admins and kicked user
        {
            std::string org_name = OrganizationService::GetOrgName(org_id);
            if (org_name.empty()) {
                org_name = std::to_string(org_id);
            }
            std::string kicked_name = OrganizationService::GetUserName(user_id);
            if (kicked_name.empty()) {
                kicked_name = std::to_string(user_id);
            }
            std::string oper_name = OrganizationService::GetUserName(viewer.user_id);

            EventOrgMemberData data = {};
            data.type = "kicked";
            data.org_id = org_id;
            data.org_name = org_name;
            data.operator_id = viewer.user_id;
            data.operator_name = oper_name;
            data.kicked_user_id = user_id;
            data.applicant_name = kicked_name;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_MEMBER, std::move(data));
        }

        result->set("success", true);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
