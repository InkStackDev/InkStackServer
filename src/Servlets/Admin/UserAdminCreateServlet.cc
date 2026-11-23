#include "UserAdminCreateServlet.hpp"

#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"
#include "DB/UserInfo.hpp"

UserAdminCreateServlet::UserAdminCreateServlet()
    : BlogAdminServlet("UserAdminCreateServlet")
{
}

int32_t UserAdminCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, account, "account");
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");
        DEFINE_AND_CHECK_TYPE(result, int32_t, role, "role");
        DEFINE_AND_CHECK_TYPE(result, int32_t, state, "state");

        if (account.empty() || passwd.empty()) {
            result->setErrno(PARAM_MISSING, "account and password required");
            break;
        }

        UserService::CreateUserInput input;
        input.account = account;
        input.email = email;
        input.passwd = passwd;
        input.role = role;
        input.state = state;

        DB::UserInfo::ptr info;
        auto rt = UserService::CreateUser(input, info);
        if (rt == UserService::CreateUserResult::Ok) {
            result->set("id", info->getId());
            result->set("account", info->getAccount());
            result->set("name", info->getName());
            result->set("email", info->getEmail());
            result->set("role", info->getRole());
            result->set("state", info->getState());
            result->set("create_time", info->getCreateTime());
        } else if (rt == UserService::CreateUserResult::AccountExists) {
            result->setErrno(USER_ACCOUNT_EXISTS);
        } else if (rt == UserService::CreateUserResult::EmailExists) {
            result->setErrno(USER_EMAIL_EXISTS);
        } else if (rt == UserService::CreateUserResult::InvalidAccount) {
            result->setErrno(USER_INVALID_ACCOUNT);
        } else if (rt == UserService::CreateUserResult::InvalidEmail) {
            result->setErrno(USER_INVALID_EMAIL);
        } else if (rt == UserService::CreateUserResult::InvalidPassword) {
            result->setErrno(USER_INVALID_PASSWORD);
        } else if (rt == UserService::CreateUserResult::InvalidRole) {
            result->setErrno(PARAM_INVALID, "invalid role");
        } else if (rt == UserService::CreateUserResult::InvalidState) {
            result->setErrno(PARAM_INVALID, "invalid state");
        } else if (rt == UserService::CreateUserResult::InternalError) {
            result->setErrno(INTERNAL_ERROR);
        } else {
            result->setErrno(DB_OPERATION_FAILED, "insert user failed");
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
