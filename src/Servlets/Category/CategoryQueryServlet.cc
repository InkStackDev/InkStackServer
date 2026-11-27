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

        bool paginated = isAdminRoute() && (request->hasParam("page_num") || request->hasParam("page_size"));
        int64_t total = 0;
        std::vector<CategoryQueryService::Item> categories;
        if (paginated) {
            uint64_t page_num = request->getParamAs<uint64_t>("page_num", 1);
            uint64_t page_size = request->getParamAs<uint64_t>("page_size", 10);
            if (page_num == 0) {
                page_num = 1;
            }
            if (page_size == 0) {
                page_size = 10;
            }
            if (page_size > 100) {
                page_size = 100;
            }
            uint64_t offset = (page_num - 1) * page_size;

            CategoryQueryService::Filter filter;
            filter.user_id = req_uid;
            filter.status = request->getParamAs<int32_t>("status", -1);
            filter.keyword = request->getParam("keyword");
            filter.sort_by = request->getParam("sort_by");
            filter.sort_order = request->getParam("sort_order");

            CategoryQueryService::Stats stats;
            if (!CategoryQueryService::ListAdmin(filter, offset, page_size, categories, total, stats)) {
                result->setErrno(DB_OPERATION_FAILED);
                break;
            }

            Json::Value stats_json;
            stats_json["total_categories"] = stats.total_categories;
            stats_json["active_categories"] = stats.active_categories;
            stats_json["total_articles"] = stats.total_articles;
            stats_json["total_views"] = stats.total_views;
            result->set("stats", stats_json);
        } else {
            if (!CategoryQueryService::List(viewer.user_id, isAdminRoute(), req_uid, categories)) {
                result->setErrno(DB_OPERATION_FAILED);
                break;
            }
            total = (int64_t)categories.size();
        }

        result->set("total", total);
        Json::Value list;
        for (const auto& cat : categories) {
            Json::Value item;
            item["id"] = cat.id;
            item["user_id"] = cat.user_id;
            item["author"] = cat.author;
            item["author_avatar"] = cat.author_avatar;
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
