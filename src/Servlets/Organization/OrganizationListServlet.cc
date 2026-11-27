#include "OrganizationListServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationListServlet::OrganizationListServlet()
    : BlogLoginedServlet("OrganizationListServlet")
{
}

int32_t OrganizationListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int32_t status = request->getParamAs<int32_t>("status", -1);

        std::vector<OrganizationService::OrganizationListItem> orgs;
        int64_t total = 0;
        if (!OrganizationService::ListOrganizations(status, 0, INT32_MAX, orgs, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        Json::Value list;
        for (const auto& org : orgs) {
            Json::Value item;
            item["id"] = org.id;
            item["name"] = org.name;
            item["status"] = org.status;
            item["type"] = org.type;
            item["description"] = org.description;
            item["count"] = org.member_count;
            item["created_at"] = org.created_at;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
