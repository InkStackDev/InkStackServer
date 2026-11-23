#include "User2FAResetServlet.hpp"

#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"

User2FAResetServlet::User2FAResetServlet()
    : BlogLoginedServlet("User2FAResetServlet")
{
}

int32_t User2FAResetServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, recoveryCode, "recovery_code");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        std::string newSecret;
        std::string otpUri;
        auto rt = UserService::Reset2FA(uid, recoveryCode, newSecret, otpUri);
        if (rt == UserService::Reset2faResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (rt == UserService::Reset2faResult::NotEnabled) {
            result->setErrno(TOTP_NOT_ENABLED);
            break;
        }
        if (rt == UserService::Reset2faResult::InvalidCode) {
            result->setErrno(RECOVERY_CODE_INVALID);
            break;
        }
        if (rt != UserService::Reset2faResult::Ok) {
            result->setErrno(TOTP_ENABLE_FAILED, "reset 2fa failed");
            break;
        }

        // 将新密钥临时存入session
        auto sdata = getSessionData(request, response);
        sdata->setData("totp_pending_secret", newSecret);

        result->set("secret", newSecret);
        result->set("otp_uri", otpUri);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
