#include "CategoryDeleteServlet.hpp"

#include <json/json.h>

#include <set>

#include "../../Manager/UserManager.hpp"
#include "../../Permission.hpp"
#include "../../Service/CategoryService.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/CategoryRepository.hpp"

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
    int64_t uid = getUserId(request);
    DB::UserInfo::ptr user;
    if (uid) {
        user = UserMgr::GetInstance()->get(uid);
    }
    int32_t rt = CheckAdminRoute(uid, user);
    if (rt) {
        result->setErrno(rt);
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

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        bool admin_route = isAdminRoute();

        std::vector<DB::CategoryInfo::ptr> infos;
        if (admin_route) {
            if (req_uid) {
                if (!CategoryRepository::ListByUserId(infos, req_uid, GetDB())) {
                    result->setErrno(DB_OPERATION_FAILED);
                    break;
                }
            } else {
                CategoryRepository::ListAll(infos, GetDB());
            }
        } else if (!CategoryRepository::ListByUserId(infos, uid, GetDB())) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (infos.empty()) {
            result->setErrno(PARAM_MISSING, "no categories");
            break;
        }

        std::vector<DB::CategoryInfo::ptr> deleted;
        if (!CategoryService::DeleteMany(infos, cat_ids, deleted)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (deleted.empty()) {
            break;
        }

        result->set("total", (int64_t)deleted.size());
        Json::Value list(Json::arrayValue);
        for (auto& info : deleted) {
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
