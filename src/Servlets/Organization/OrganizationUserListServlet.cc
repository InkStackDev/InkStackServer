#include "OrganizationUserListServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationUserListServlet::OrganizationUserListServlet()
    : BlogLoginedServlet("OrganizationUserListServlet")
{
}

int32_t OrganizationUserListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;

        std::vector<OrganizationService::OrganizationMemberRow> rows;
        int64_t total = 0;
        auto rt = OrganizationService::ListMemberPage(viewer, id, status, offset, page_size, rows, total);
        if (rt == OrganizationService::ListResult::NotMember) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        if (rt == OrganizationService::ListResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationService::ListResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        Json::Value list;
        for (const auto& row : rows) {
            Json::Value item;
            item["id"] = row.id;
            item["user_id"] = row.user_id;
            item["name"] = row.name;
            item["avatar"] = row.avatar;
            item["email"] = row.email;
            item["role"] = row.role;
            item["status"] = row.status;
            item["join_time"] = row.join_time;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
