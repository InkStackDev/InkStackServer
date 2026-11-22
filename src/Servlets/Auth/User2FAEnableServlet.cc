#include "User2FAEnableServlet.hpp"

#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"

User2FAEnableServlet::User2FAEnableServlet()
    : BlogLoginedServlet("User2FAEnableServlet")
{
}

int32_t User2FAEnableServlet::handle(chen::http::HttpRequest::ptr request,
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

        // 生成新密钥（不立即写入DB，等用户confirm后才正式启用）
        std::string secret;
        std::string otpUri;
        auto rt = UserService::BeginEnable2FA(uid, secret, otpUri);
        if (rt == UserService::Begin2faResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (rt == UserService::Begin2faResult::AlreadyEnabled) {
            result->setErrno(TOTP_ALREADY_ENABLED);
            break;
        }

        // 将密钥临时存入session，等待confirm步骤确认
        auto sdata = getSessionData(request, response);
        sdata->setData("totp_pending_secret", secret);

        result->set("secret", secret);
        result->set("otp_uri", otpUri);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
