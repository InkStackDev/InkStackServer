#include "UserAdminDeleteServlet.hpp"

#include <utility>

#include "../../Auth/AuthService.hpp"
#include "../../Event/EventDefine.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"

UserAdminDeleteServlet::UserAdminDeleteServlet()
    : BlogAdminServlet("UserAdminDeleteServlet")
{
}

int32_t UserAdminDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");

        auto rt = UserService::DeleteUser(user_id);
        if (rt == UserService::DeleteUserResult::Ok) {
            AuthService::RevokeAllSessions(user_id);
            result->set("user_id", user_id);

            EventUserAdminData data = {};
            data.type = "account_deleted";
            data.user_id = user_id;
            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_USER_ADMIN, std::move(data));
        } else if (rt == UserService::DeleteUserResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
        } else if (rt == UserService::DeleteUserResult::AlreadyDeleted) {
            result->setErrno(USER_ALREADY_DELETED);
        } else {
            result->setErrno(DB_OPERATION_FAILED, "delete user failed");
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
