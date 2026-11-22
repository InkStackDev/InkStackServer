#include "OrganizationJoinServlet.hpp"

#include <json/json.h>

#include "../../Event/EventDefine.hpp"
#include "../../Manager/OrganizationManager.hpp"
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

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::OrganizationUserRelInfo::ptr info;
        auto rt = OrganizationService::Join(id, uid, info);
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

        auto org = OrganizationMgr::GetInstance()->get(id);
        if (!org) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }

        // Notify org admins about join request
        {
            EventOrgMemberData data = {};
            data.type = "join_request";
            data.org_id = id;
            data.org_name = org->getName();
            data.applicant_id = uid;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_MEMBER, std::move(data));
        }

        result->set("id", org->getId());
        result->set("name", org->getName());
        result->set("description", org->getDescription());
        result->set("type", org->getType());
        result->set("status", org->getStatus());
        result->set("user_in_org", info->getStatus());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
