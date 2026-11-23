#include "RdOrganizationsServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

RdOrganizationsServlet::RdOrganizationsServlet()
    : BlogLoginedServlet("RdOrganizationsServlet")
{
}

int32_t RdOrganizationsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        std::vector<OrganizationService::UserOrganizationView> orgs;
        if (!OrganizationService::ListUserOrganizations(viewer, orgs)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& org : orgs) {
            Json::Value item;
            item["org_id"] = org.org_id;
            item["org_name"] = org.name;
            item["role"] = org.role;
            arr.append(item);
        }

        result->set("list", arr);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
