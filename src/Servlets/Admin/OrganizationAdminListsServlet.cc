#include "OrganizationAdminListsServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationAdminListsServlet::OrganizationAdminListsServlet()
    : BlogAdminServlet("OrganizationAdminListsServlet")
{
}

int32_t OrganizationAdminListsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        int32_t status = request->getParamAs<int32_t>("status", -1);

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<OrganizationService::OrganizationListItem> orgs;
        int64_t total = 0;
        if (!OrganizationService::ListOrganizations(status, offset, page_size, orgs, total)) {
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
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
