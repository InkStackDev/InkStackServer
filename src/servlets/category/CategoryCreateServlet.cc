#include "CategoryCreateServlet.hpp"

#include "../../Manager/UserManager.hpp"
#include "../../Permission.hpp"
#include "../../Service/CategoryService.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/CategoryRepository.hpp"

CategoryCreateServlet::CategoryCreateServlet()
    : BlogLoginedServlet("CategoryCreateServlet")
{
}

bool CategoryCreateServlet::handlePre(
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

int32_t CategoryCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, url, "url");
        DEFINE_AND_CHECK_STRING(result, icon, "icon");
        DEFINE_AND_CHECK_STRING(result, color, "color");
        std::string desc = request->getParamAs<std::string>("desc");
        int64_t parent_id = request->getParamAs<int64_t>("parent_id");
        int32_t status = request->getParamAs<int32_t>("status", 1);
        int64_t cid = request->getParamAs<int64_t>("id", 0);
        int64_t req_uid = request->getParamAs<int64_t>("user_id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        bool admin_route = isAdminRoute();

        int64_t owner_id = uid;
        if (cid) {
            auto existing = CategoryRepository::Get(cid, GetDB());
            if (!existing) {
                result->setErrno(PARAM_INVALID, "invalid cid");
                break;
            }
            if (!admin_route && existing->getUserId() != uid) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            owner_id = existing->getUserId();
        } else if (req_uid && req_uid != uid) {
            if (!admin_route) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            owner_id = req_uid;
            if (!UserMgr::GetInstance()->get(owner_id)) {
                result->setErrno(USER_NOT_FOUND);
                break;
            }
        }

        CategoryInput input;
        input.id = cid;
        input.owner_id = owner_id;
        input.name = name;
        input.color = color;
        input.description = desc;
        input.url = url;
        input.icon = icon;
        input.parent_id = parent_id;
        input.status = status;

        DB::CategoryInfo::ptr info;
        auto rt = CategoryService::CreateOrUpdate(input, uid, admin_route, info);
        if (rt == CategoryService::Result::NotFound) {
            result->setErrno(PARAM_INVALID, "invalid cid");
            break;
        }
        if (rt == CategoryService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == CategoryService::Result::InvalidParent) {
            result->setErrno(PARAM_INVALID, "invalid parent_id");
            break;
        }
        if (rt == CategoryService::Result::NameExists) {
            result->setErrno(PARAM_INVALID, "category name exists");
            break;
        }
        if (rt != CategoryService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED, "category operation failed");
            break;
        }

        result->set("id", info->getId());
        result->set("user_id", info->getUserId());
        result->set("name", info->getName());
        result->set("color", info->getColor());
        result->set("parent_id", parent_id);
        result->set("url", info->getUrl());
        result->set("icon", info->getIcon());
        result->set("desc", info->getDescription());
        result->set("status", info->getStatus());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
