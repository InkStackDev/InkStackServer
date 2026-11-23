#include "UserAdminUpdateServlet.hpp"

#include "../../Auth/AuthService.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"
#include "DB/UserInfo.hpp"

UserAdminUpdateServlet::UserAdminUpdateServlet()
    : BlogAdminServlet("UserAdminUpdateServlet")
{
}

int32_t UserAdminUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");

        UserService::AdminUpdateInput input;
        input.account = request->getParam("account");
        input.email = request->getParam("email");
        input.passwd = request->getParam("passwd");
        input.has_role = request->checkGetParamAs("role", input.role);
        input.has_state = request->checkGetParamAs("state", input.state);

        DB::UserInfo::ptr info;
        bool revoke_sessions = false;
        auto rt = UserService::AdminUpdate(user_id, input, info, revoke_sessions);
        if (rt == UserService::AdminUpdateResult::Ok) {
            if (revoke_sessions) {
                AuthService::RevokeAllSessions(user_id);
            }
            result->set("id", info->getId());
            result->set("account", info->getAccount());
            result->set("name", info->getName());
            result->set("email", info->getEmail());
            result->set("role", info->getRole());
            result->set("state", info->getState());
            result->set("update_time", info->getUpdateTime());
        } else if (rt == UserService::AdminUpdateResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
        } else if (rt == UserService::AdminUpdateResult::NoParam) {
            result->setErrno(USER_NO_PARAM);
        } else if (rt == UserService::AdminUpdateResult::InvalidAccount) {
            result->setErrno(USER_INVALID_ACCOUNT);
        } else if (rt == UserService::AdminUpdateResult::AccountExists) {
            result->setErrno(USER_ACCOUNT_EXISTS);
        } else if (rt == UserService::AdminUpdateResult::InvalidEmail) {
            result->setErrno(USER_INVALID_EMAIL);
        } else if (rt == UserService::AdminUpdateResult::EmailExists) {
            result->setErrno(USER_EMAIL_EXISTS);
        } else if (rt == UserService::AdminUpdateResult::InvalidPassword) {
            result->setErrno(USER_INVALID_PASSWORD);
        } else if (rt == UserService::AdminUpdateResult::InvalidRole) {
            result->setErrno(PARAM_INVALID, "invalid role");
        } else if (rt == UserService::AdminUpdateResult::InvalidState) {
            result->setErrno(PARAM_INVALID, "invalid state");
        } else if (rt == UserService::AdminUpdateResult::InternalError) {
            result->setErrno(INTERNAL_ERROR);
        } else {
            result->setErrno(DB_OPERATION_FAILED, "update user failed");
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
