#include "CategoryQueryServlet.hpp"

#include "../../Service/CategoryQueryService.hpp"
#include "../../Util/Util.hpp"

CategoryQueryServlet::CategoryQueryServlet()
    : BlogLoginedServlet("CategoryQueryServlet")
{
}

bool CategoryQueryServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!BlogLoginedServlet::handlePre(request, response, session, result)) {
        return false;
    }
    if (!isAdminRoute()) {
        return true;
    }
    Viewer viewer = getViewer(request);
    if (!viewer.valid()) {
        result->setErrno(NOT_LOGIN);
        return false;
    }
    if (!viewer.is_platform_admin) {
        result->setErrno(ACCESS_DENIED);
        return false;
    }
    return true;
}

int32_t CategoryQueryServlet::handle(
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
        int64_t req_uid = request->getParamAs<int64_t>("user_id", 0);

        std::vector<CategoryQueryService::Item> categories;
        if (!CategoryQueryService::List(viewer.user_id, isAdminRoute(), req_uid, categories)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", (int64_t)categories.size());
        Json::Value list;
        for (const auto& cat : categories) {
            Json::Value item;
            item["id"] = cat.id;
            item["user_id"] = cat.user_id;
            item["author"] = cat.author;
            item["name"] = cat.name;
            item["url"] = cat.url;
            item["icon"] = cat.icon;
            item["color"] = cat.color;
            item["description"] = cat.description;
            item["parent_id"] = cat.parent_id;
            item["status"] = cat.status;
            item["create_time"] = cat.create_time;
            item["update_time"] = cat.update_time;
            item["article_count"] = cat.article_count;
            item["view_count"] = cat.view_count;
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
