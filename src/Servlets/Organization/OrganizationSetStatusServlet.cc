#include "OrganizationSetStatusServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationSetStatusServlet::OrganizationSetStatusServlet()
    : BlogLoginedServlet("OrganizationSetStatusServlet")
{
}

int32_t OrganizationSetStatusServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::OrganizationInfo::ptr info;
        auto rt = OrganizationService::SetStatus(viewer, id, status, info);
        if (rt == OrganizationService::StatusResult::NotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt == OrganizationService::StatusResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationService::StatusResult::InvalidStatus) {
            result->setErrno(ORG_INVALID_STATE);
            break;
        }
        if (rt != OrganizationService::StatusResult::Ok) {
            result->setErrno(ORG_UPDATE_FAILED);
            break;
        }
        if (!info) {
            result->setErrno(INTERNAL_ERROR);
            break;
        }

        result->set("id", info->getId());
        result->set("status", info->getStatus());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
