#include "UserOrganizationListServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

UserOrganizationListServlet::UserOrganizationListServlet()
    : BlogLoginedServlet("UserOrganizationListServlet")
{
}

int32_t UserOrganizationListServlet::handle(
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

        std::vector<UserQueryService::OrganizationItem> orgs;
        if (!UserQueryService::ListOrganizations(viewer.user_id, orgs)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list;
        for (const auto& org : orgs) {
            Json::Value item;
            item["id"] = org.rel_id;
            item["org_id"] = org.org_id;
            item["org_name"] = org.org_name;
            item["org_description"] = org.org_description;
            item["type"] = org.type;
            item["role"] = org.role;
            item["join_time"] = org.join_time;
            item["count"] = org.member_count;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
