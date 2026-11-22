#include "OrganizationApplyReviewServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationApplyReviewServlet::OrganizationApplyReviewServlet()
    : BlogLoginedServlet("OrganizationApplyReviewServlet")
{
}

int32_t OrganizationApplyReviewServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, apply_id_str, "apply_id");
        DEFINE_AND_CHECK_STRING(result, action, "action");
        std::string reason = request->getParamAs<std::string>("reason");

        int64_t apply_id = 0;
        try {
            apply_id = chen::TypeUtil::Atoi(apply_id_str);
        } catch (...) {
            result->setErrno(PARAM_INVALID, "invalid apply_id");
            break;
        }

        if (action != "approve" && action != "reject") {
            result->setErrno(PARAM_INVALID, "action must be approve or reject");
            break;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        bool approve = action == "approve";
        DB::OrganizationInfo::ptr org;
        DB::OrganizationApplyInfo::ptr apply;
        auto rt = OrganizationService::ReviewApply(viewer, apply_id, approve, reason, org, apply);
        if (rt == OrganizationService::Result::NotFound) {
            result->setErrno(ORG_APPLY_NOT_FOUND);
            break;
        }
        if (rt == OrganizationService::Result::Conflict) {
            result->setErrno(ORG_APPLY_ALREADY_REVIEWED);
            break;
        }
        if (rt == OrganizationService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationService::Result::Ok) {
            result->setErrno(approve ? ORG_INSERT_FAILED : ORG_APPLY_UPDATE_FAILED);
            break;
        }

        if (approve && org) {
            result->set("org_id", org->getId());

            EventOrgApplyData data = {};
            data.type = "approved";
            data.applicant_id = apply->getUserId();
            data.org_name = apply->getOrgName();
            data.org_id = org->getId();

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_APPLY, std::move(data));
        } else {
            EventOrgApplyData data = {};
            data.type = "rejected";
            data.applicant_id = apply->getUserId();
            data.org_name = apply->getOrgName();
            data.reason = reason;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ORG_APPLY, std::move(data));
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
