#include "OrganizationDetailServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationDetailServlet::OrganizationDetailServlet()
    : BlogLoginedServlet("OrganizationDetailServlet")
{
}

int32_t OrganizationDetailServlet::handle(
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

        OrganizationService::OrganizationDetailView detail;
        auto rt = OrganizationService::GetDetail(viewer, id, detail);
        if (rt == OrganizationService::Result::NotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt == OrganizationService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (detail.has_membership) {
            result->set("user_in_org", detail.member_status);
            result->set("user_role", detail.member_role);
        }

        result->set("id", detail.id);
        result->set("name", detail.name);
        result->set("description", detail.description);
        result->set("type", detail.type);
        result->set("status", detail.status);
        result->set("owner_id", detail.owner_id);
        result->set("is_owner", detail.is_owner);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
