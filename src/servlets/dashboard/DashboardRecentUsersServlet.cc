#include "DashboardRecentUsersServlet.hpp"

#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

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
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserRepository::Get(uid, GetDB());
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();
        if (role != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        int32_t limit = request->getParamAs<int32_t>("limit", 5);
        if (limit <= 0) {
            limit = 5;
        }

        std::vector<int64_t> ids;
        UserRepository::GetAllIds(ids, true, GetDB());

        std::vector<DB::UserInfo::ptr> users;
        for (auto id : ids) {
            auto u = UserRepository::Get(id, GetDB());
            if (u) {
                users.push_back(u);
            }
        }

        std::sort(users.begin(), users.end(), [](auto& a, auto& b) {
            return a->getCreateTime() > b->getCreateTime();
        });

        if ((int32_t)users.size() > limit) {
            users.resize(limit);
        }

        Json::Value list(Json::arrayValue);
        for (auto& u : users) {
            Json::Value item;
            item["name"] = u->getName();
            item["role"] = RoleToString(u->getRole());
            item["avatar"] = u->getAvatar();
            item["status"] = StatusToString(u->getState());
            list.append(item);
        }

        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
