#include "UserLoginServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"
#include "../../Service/UserService.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";
/// 2FA 临时凭证 TTL（秒）
static const int k2faPendingTTL = 300;

UserLoginServlet::UserLoginServlet()
    : BlogServlet("UserLoginServlet")
{
}

int32_t UserLoginServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, auth_id, "auth_id");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");

        AuthContext ctx = AuthService::Resolve(request, response, false);
        if (ctx.valid()) {
            result->setErrno(USER_ALREADY_LOGIN);
            break;
        }

        DB::UserInfo::ptr info;
        if (IsEmail(auth_id)) {
            info = UserService::GetByEmail(auth_id);
        } else if (IsValidAccount(auth_id)) {
            info = UserService::GetByAccount(auth_id);
        } else {
            result->setErrno(USER_INVALID_ACCOUNT);
            break;
        }

        if (!info) {
            result->setErrno(AUTH_CODE_INVALID);
            break;
        }
        if (!VerifyPassword(passwd, info->getPasswd())) {
            result->setErrno(USER_PASSWORD_WRONG);
            break;
        }

        if (info->getState() != 1 || info->getIsDeleted()) {
            result->setErrno(ACCOUNT_INVALID);
            break;
        }

        // 历史 MD5 哈希在登录成功后透明升级为 bcrypt；失败不影响本次登录，下次登录重试
        if (IsLegacyPasswordHash(info->getPasswd())) {
            std::string passwd_hash = HashPassword(passwd);
            if (!passwd_hash.empty()) {
                std::string old_hash = info->getPasswd();
                if (UserService::UpgradePasswordHash(info->getId(), old_hash, passwd_hash)) {
                    info->setPasswd(passwd_hash);
                } else {
                    ERROR(logger) << "upgrade password hash failed user=" << info->getId();
                }
            }
        }

        // 2FA: 密码验证通过后，如果启用了TOTP，返回临时凭证等待第二步验证
        if (info->getTotpEnabled()) {
            std::string tempToken = UserService::GenerateToken();
            std::string redisKey = std::string(k2faPendingPrefix) + tempToken;
            chen::RedisUtil::Cmd(
                GetRedisPoolName(), "setex %s %d %lld", redisKey.c_str(), k2faPendingTTL, (long long)info->getId());
            result->set("require_2fa", true);
            result->set("temp_token", tempToken);
            INFO(logger) << "login user=" << info->getId() << " require 2fa verification";
            break;
        }

        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, info, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }
        INFO(logger) << "login user=" << info->getId();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
