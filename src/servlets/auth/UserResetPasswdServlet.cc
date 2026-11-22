#include "UserResetPasswdServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"

UserResetPasswdServlet::UserResetPasswdServlet()
    : BlogServlet("UserResetPasswdServlet")
{
}

int32_t UserResetPasswdServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");
        DEFINE_AND_CHECK_STRING(result, auth_code, "auth_code");

        if (passwd.empty()) {
            result->setErrno(PARAM_MISSING, "password required");
            break;
        }

        if (!IsEmail(email)) {
            result->setErrno(USER_INVALID_EMAIL);
            break;
        }

        if (!verificationEmailCode(email, auth_code)) {
            result->setErrno(AUTH_CODE_INVALID);
            break;
        }

        DB::UserInfo::ptr info;
        auto rt = UserService::SetPasswordByEmail(email, passwd, info);
        if (rt == UserService::SetPasswordResult::Ok) {
            AuthService::RevokeAllSessions(info->getId());
        } else if (rt == UserService::SetPasswordResult::UserNotFound) {
            result->setErrno(USER_EMAIL_NOT_REGISTER);
        } else if (rt == UserService::SetPasswordResult::InvalidPassword) {
            result->setErrno(USER_INVALID_PASSWORD);
        } else if (rt == UserService::SetPasswordResult::InternalError) {
            result->setErrno(INTERNAL_ERROR);
        } else {
            result->setErrno(DB_OPERATION_FAILED, "reset password failed");
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

bool UserResetPasswdServlet::verificationEmailCode(const std::string& email, const std::string& code)
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "GET email:verify:3:%s", email.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING) {
        return false;
    }
    if (code != rpy->str) {
        return false;
    }
    // 一次性使用，校验通过后删除
    chen::RedisUtil::Cmd(GetRedisPoolName(), "DEL email:verify:3:%s", email.c_str());
    return true;
}
