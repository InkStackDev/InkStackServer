#include "User2FARecoveryServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";

User2FARecoveryServlet::User2FARecoveryServlet()
    : BlogServlet("User2FARecoveryServlet")
{
}

int32_t User2FARecoveryServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, tempToken, "temp_token");
        DEFINE_AND_CHECK_STRING(result, recoveryCode, "recovery_code");

        // 从Redis获取临时凭证对应的用户ID
        std::string key = std::string(k2faPendingPrefix) + tempToken;
        auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", key.c_str());
        int64_t uid = 0;
        if (rpy && rpy->type == REDIS_REPLY_STRING && rpy->str) {
            try {
                uid = chen::TypeUtil::Atoi(std::string(rpy->str, rpy->len));
            } catch (...) {
                uid = 0;
            }
        }
        if (!uid) {
            result->setErrno(TEMP_TOKEN_INVALID);
            break;
        }

        // 删除临时凭证（一次性）
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", key.c_str());

        auto rt = UserService::ConsumeRecoveryCode(uid, recoveryCode);
        if (rt == UserService::ConsumeRecoveryResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (rt == UserService::ConsumeRecoveryResult::AccountInvalid) {
            result->setErrno(ACCOUNT_INVALID);
            break;
        }
        if (rt == UserService::ConsumeRecoveryResult::NotEnabled) {
            result->setErrno(TOTP_NOT_ENABLED);
            break;
        }
        if (rt == UserService::ConsumeRecoveryResult::InvalidCode) {
            result->setErrno(RECOVERY_CODE_INVALID);
            break;
        }
        if (rt != UserService::ConsumeRecoveryResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        auto info = UserService::Get(uid);
        if (!info) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        // 完成登录
        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, info, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
