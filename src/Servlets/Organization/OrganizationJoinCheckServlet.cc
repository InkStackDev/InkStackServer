#include "OrganizationJoinCheckServlet.hpp"

#include <json/json.h>

#include "../../Event/EventDefine.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"

OrganizationJoinCheckServlet::OrganizationJoinCheckServlet()
    : BlogLoginedServlet("OrganizationJoinCheckServlet")
{
}

int32_t OrganizationJoinCheckServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_TYPE(result, int32_t, state, "state");

        if (state != OrgMemberStatus::REJECTED && state != OrgMemberStatus::APPROVED) {
            result->setErrno(PARAM_INVALID, "invalid state");
            break;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::OrganizationUserRelInfo::ptr rel;
        auto rt = OrganizationService::SetMemberState(viewer, org_id, user_id, state, rel);
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
        if (rt == OrganizationService::MemberResult::InvalidState) {
            result->setErrno(ORG_INVALID_STATE);
            break;
        }
        if (rt != OrganizationService::MemberResult::Ok) {
            result->setErrno(ORG_USER_REL_FAILED);
            break;
        }

        // Notify applicant
        {
            EventOrgMemberData data = {};
            data.type = (state == OrgMemberStatus::APPROVED) ? "join_approved" : "join_rejected";
            data.org_id = org_id;
            data.org_name = OrganizationService::GetOrgName(org_id);
            data.applicant_id = user_id;
            data.operator_id = viewer.user_id;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_MEMBER, std::move(data));
        }

        result->set("success", true);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
