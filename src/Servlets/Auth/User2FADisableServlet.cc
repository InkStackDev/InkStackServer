#include "User2FADisableServlet.hpp"

#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"

User2FADisableServlet::User2FADisableServlet()
    : BlogLoginedServlet("User2FADisableServlet")
{
}

int32_t User2FADisableServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, code, "code");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto rt = UserService::Disable2FA(uid, code);
        if (rt == UserService::Disable2faResult::Ok) {
            result->setErrno(SUCCESS);
        } else if (rt == UserService::Disable2faResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
        } else if (rt == UserService::Disable2faResult::NotEnabled) {
            result->setErrno(TOTP_NOT_ENABLED);
        } else if (rt == UserService::Disable2faResult::InvalidCode) {
            result->setErrno(TOTP_INVALID_CODE);
        } else {
            result->setErrno(TOTP_ENABLE_FAILED, "disable 2fa failed");
            break;
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
