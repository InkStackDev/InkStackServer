#include "DashboardRecentUsersServlet.hpp"

#include "../../Domain/UserTypes.hpp"
#include "../../Service/DashboardService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

DashboardRecentUsersServlet::DashboardRecentUsersServlet()
    : BlogLoginedServlet("DashboardRecentUsersServlet")
{
}

static const char* RoleToString(int32_t role)
{
    switch (role) {
    case UserRole::USER:
        return "user";
    case UserRole::ADMIN:
        return "admin";
    case UserRole::EDITOR:
        return "editor";
    case UserRole::CHECKER:
        return "checker";
    default:
        return "unknown";
    }
}

static const char* StatusToString(int32_t state)
{
    switch (state) {
    case UserState::INACTIVE:
        return "inactive";
    case UserState::ACTIVE:
        return "active";
    case UserState::BANNED:
        return "banned";
    default:
        return "unknown";
    }
}

int32_t DashboardRecentUsersServlet::handle(
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
        if (!viewer.is_platform_admin) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        int32_t limit = request->getParamAs<int32_t>("limit", 5);
        std::vector<DashboardService::RecentUser> users;
        if (!DashboardService::GetRecentUsers(limit, users)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value list(Json::arrayValue);
        for (const auto& user : users) {
            Json::Value item;
            item["name"] = user.name;
            item["role"] = RoleToString(user.role);
            item["avatar"] = user.avatar;
            item["status"] = StatusToString(user.state);
            list.append(item);
        }

        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
