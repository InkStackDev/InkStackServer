#include "EmailRecoveryServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// 账号恢复验证码 Redis key 前缀（type=5）
static const char* kRecoveryCodePrefix = "email:verify:5:";
/// 账号恢复验证码错误次数 key 前缀
static const char* kRecoveryFailPrefix = "email:verify:fail:5:";
/// 验证码错误次数上限
static const int kRecoveryFailLimit = 5;

EmailRecoveryServlet::EmailRecoveryServlet()
    : BlogServlet("EmailRecoveryServlet")
{
}

int32_t EmailRecoveryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, auth_code, "auth_code");

        if (!IsEmail(email)) {
            result->setErrno(USER_INVALID_EMAIL);
            break;
        }

        auto user = UserRepository::GetByEmail(email, GetDB());
        if (!user) {
            result->setErrno(USER_EMAIL_NOT_REGISTER);
            break;
        }
        if (user->getState() != UserState::ACTIVE || user->getIsDeleted()) {
            result->setErrno(ACCOUNT_INVALID);
            break;
        }

        std::string fail_key = std::string(kRecoveryFailPrefix) + email;
        auto fail_rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", fail_key.c_str());
        if (fail_rpy && fail_rpy->type == REDIS_REPLY_STRING && fail_rpy->str) {
            int64_t failures = 0;
            try {
                failures = chen::TypeUtil::Atoi(std::string(fail_rpy->str, fail_rpy->len));
            } catch (...) {
                failures = 0;
            }
            if (failures >= kRecoveryFailLimit) {
                result->setErrno(EMAIL_CODE_ATTEMPTS_EXCEEDED);
                break;
            }
        }

        std::string code_key = std::string(kRecoveryCodePrefix) + email;
        auto code_rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "get %s", code_key.c_str());
        bool matched = code_rpy && code_rpy->type == REDIS_REPLY_STRING && code_rpy->str && auth_code == std::string(code_rpy->str, code_rpy->len);
        if (!matched) {
            auto inc_rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "incr %s", fail_key.c_str());
            if (inc_rpy && inc_rpy->integer == 1) {
                chen::RedisUtil::Cmd(GetRedisPoolName(), "expire %s 600", fail_key.c_str());
            }
            result->setErrno(AUTH_CODE_INVALID);
            break;
        }

        // 验证码一次性使用。
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", code_key.c_str());
        chen::RedisUtil::Cmd(GetRedisPoolName(), "del %s", fail_key.c_str());

        // 账号恢复属于敏感操作，撤销全部旧会话后重新签发。
        AuthService::RevokeAllSessions(user->getId());

        IssueOptions options;
        int32_t ret = AuthService::IssueSession(request, response, session, user, options);
        if (ret != 0) {
            result->setErrno(ret);
            break;
        }

        result->set("id", user->getId());
        result->set("account", user->getAccount());
        result->set("name", user->getName());
        result->set("avatar", user->getAvatar());
        result->set("role", user->getRole());
        result->setErrno(SUCCESS);
        INFO(logger) << "email recovery login uid=" << user->getId();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
