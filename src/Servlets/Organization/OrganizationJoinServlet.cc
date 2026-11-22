#include "OrganizationJoinServlet.hpp"

#include <json/json.h>

#include "../../Event/EventDefine.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationJoinServlet::OrganizationJoinServlet()
    : BlogLoginedServlet("OrganizationJoinServlet")
{
}

int32_t OrganizationJoinServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::OrganizationUserRelInfo::ptr info;
        auto rt = OrganizationService::Join(id, viewer.user_id, info);
        if (rt == OrganizationService::JoinResult::OrgNotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }
        if (rt == OrganizationService::JoinResult::OrgDisabled) {
            result->setErrno(ORG_DISABLED);
            break;
        }
        if (rt == OrganizationService::JoinResult::AlreadyJoined) {
            result->setErrno(ORG_ALREADY_JOINED);
            break;
        }
        if (rt == OrganizationService::JoinResult::AlreadyApplied) {
            result->setErrno(ORG_ALREADY_APPLIED);
            break;
        }
        if (rt != OrganizationService::JoinResult::Ok) {
            result->setErrno(ORG_UPDATE_FAILED);
            break;
        }

        OrganizationService::OrganizationDetailView detail;
        auto detail_rt = OrganizationService::GetDetail(viewer, id, detail);
        if (detail_rt == OrganizationService::Result::NotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }
        if (detail_rt != OrganizationService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        // Notify org admins about join request
        {
            EventOrgMemberData data = {};
            data.type = "join_request";
            data.org_id = id;
            data.org_name = detail.name;
            data.applicant_id = viewer.user_id;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_MEMBER, std::move(data));
        }

        result->set("id", detail.id);
        result->set("name", detail.name);
        result->set("description", detail.description);
        result->set("type", detail.type);
        result->set("status", detail.status);
        result->set("user_in_org", detail.member_status);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
