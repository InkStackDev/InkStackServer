#include "User2FAVerifyServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/TotpUtil.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";

User2FAVerifyServlet::User2FAVerifyServlet()
    : BlogServlet("User2FAVerifyServlet")
{
}

int32_t User2FAVerifyServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, tempToken, "temp_token");
        DEFINE_AND_CHECK_STRING(result, code, "code");

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
            result->setErrno(errcode::TEMP_TOKEN_INVALID);
            break;
        }

        // 删除临时凭证（一次性）
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", key.c_str());

        auto info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        if (info->getState() != 1 || info->getIsDeleted()) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        // 验证TOTP码
        if (!TotpUtil::VerifyCode(info->getTotpSecret(), code, 1)) {
            result->setErrno(errcode::TOTP_INVALID_CODE);
            break;
        }

        // TOTP验证通过，签发会话
        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, info, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
        INFO(logger) << "2fa login user=" << info->getId();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
