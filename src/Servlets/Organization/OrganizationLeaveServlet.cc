#include "OrganizationLeaveServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationLeaveServlet::OrganizationLeaveServlet()
    : BlogLoginedServlet("OrganizationLeaveServlet")
{
}

int32_t OrganizationLeaveServlet::handle(chen::http::HttpRequest::ptr request,
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

        auto rt = OrganizationService::Leave(viewer, id);
        if (rt == OrganizationService::LeaveResult::OrgNotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt == OrganizationService::LeaveResult::OwnerCannotLeave) {
            result->setErrno(ORG_OWNER_CANNOT_LEAVE);
            break;
        }
        if (rt == OrganizationService::LeaveResult::NotMember) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        if (rt != OrganizationService::LeaveResult::Ok) {
            result->setErrno(ORG_USER_REL_FAILED);
            break;
        }

        result->set("success", true);
        result->set("org_id", id);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
