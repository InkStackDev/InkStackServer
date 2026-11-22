#include "CategoryDeleteServlet.hpp"

#include "../../Service/CategoryService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <set>

CategoryDeleteServlet::CategoryDeleteServlet()
    : BlogLoginedServlet("CategoryDeleteServlet")
{
}

bool CategoryDeleteServlet::handlePre(
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

int32_t CategoryDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> cat_ids;
        for (auto& i : chen::StringUtil::Split(ids, ",")) {
            if (!i.empty()) {
                cat_ids.insert(chen::TypeUtil::Atoi(i));
            }
        }
        int64_t req_uid = request->getParamAs<int64_t>("user_id", 0);

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        std::vector<DB::CategoryInfo::ptr> deleted;
        auto rt = CategoryService::DeleteForUser(isAdminRoute(), viewer.user_id, req_uid, cat_ids, deleted);
        if (rt == CategoryService::DeleteScopeResult::NoCategories) {
            result->setErrno(PARAM_MISSING, "no categories");
            break;
        }
        if (rt != CategoryService::DeleteScopeResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (deleted.empty()) {
            break;
        }

        result->set("total", (int64_t)deleted.size());
        Json::Value list(Json::arrayValue);
        for (const auto& info : deleted) {
            Json::Value item;
            item["id"] = info->getId();
            item["user_id"] = info->getUserId();
            item["name"] = info->getName();
            item["color"] = info->getColor();
            item["parent_id"] = info->getParentId();
            item["url"] = info->getUrl();
            item["icon"] = info->getIcon();
            item["desc"] = info->getDescription();
            item["status"] = info->getStatus();
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
