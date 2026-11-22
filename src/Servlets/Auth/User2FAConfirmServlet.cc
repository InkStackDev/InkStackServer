#include "User2FAConfirmServlet.hpp"

#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"

User2FAConfirmServlet::User2FAConfirmServlet()
    : BlogLoginedServlet("User2FAConfirmServlet")
{
}

int32_t User2FAConfirmServlet::handle(chen::http::HttpRequest::ptr request,
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

        // 从session获取待确认的密钥
        auto sdata = getSessionData(request, response);
        std::string secret = sdata->getData<std::string>("totp_pending_secret");
        if (secret.empty()) {
            result->setErrno(TOTP_ENABLE_FAILED, "please call /api/v1/user/2fa/enable first");
            break;
        }

        std::vector<std::string> recoveryCodes;
        auto rt = UserService::ConfirmEnable2FA(uid, secret, code, recoveryCodes);
        if (rt == UserService::Confirm2faResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (rt == UserService::Confirm2faResult::AlreadyEnabled) {
            result->setErrno(TOTP_ALREADY_ENABLED);
            break;
        }
        if (rt == UserService::Confirm2faResult::SecretMissing) {
            result->setErrno(TOTP_ENABLE_FAILED, "please call /api/v1/user/2fa/enable first");
            break;
        }
        if (rt == UserService::Confirm2faResult::InvalidCode) {
            result->setErrno(TOTP_INVALID_CODE);
            break;
        }
        if (rt != UserService::Confirm2faResult::Ok) {
            result->setErrno(TOTP_ENABLE_FAILED);
            break;
        }

        // 清除session中的待确认密钥
        sdata->setData("totp_pending_secret", std::string(""));

        // 返回恢复码（仅此一次展示）
        Json::Value codesArr;
        for (std::string& c : recoveryCodes) {
            codesArr.append(c);
        }
        result->set("recovery_codes", codesArr);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
