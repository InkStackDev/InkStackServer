#include "OrganizationMyAppliesServlet.hpp"

#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

OrganizationMyAppliesServlet::OrganizationMyAppliesServlet()
    : BlogLoginedServlet("OrganizationMyAppliesServlet")
{
}

int32_t OrganizationMyAppliesServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_size, "page_size");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, page_num, "page_num");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        uint64_t offset = (page_num - 1) * page_size;
        std::vector<OrganizationService::OrganizationApplyView> applies;
        int64_t total = 0;
        auto rt = OrganizationService::ListMyApplies(viewer.user_id, offset, page_size, applies, total);
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
        for (const auto& apply : applies) {
            Json::Value item;
            item["id"] = apply.id;
            item["user_id"] = apply.user_id;
            item["user_name"] = apply.user_name;
            item["user_avatar"] = apply.user_avatar;
            item["org_name"] = apply.org_name;
            item["org_type"] = apply.org_type;
            item["org_description"] = apply.org_description;
            item["status"] = apply.status;
            item["review_reason"] = apply.review_reason;
            item["created_at"] = apply.created_at;
            item["reviewed_at"] = apply.reviewed_at;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
