#include "UserCreateServlet.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/DB/Redis.hpp>
#include <Chen/Log/Log.hpp>

#include "../../Manager/SystemSettingsManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"
#include "../../Config/AppConfig.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

UserCreateServlet::UserCreateServlet()
    : BlogServlet("UserCreateServlet")
{
}

int32_t UserCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, account, "account");
        DEFINE_AND_CHECK_STRING(result, email, "email");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");
        DEFINE_AND_CHECK_STRING(result, auth_code, "auth_code");

        if (account.empty() || passwd.empty()) {
            result->setErrno(PARAM_MISSING, "account and password required");
            break;
        }

        if (UserMgr::GetInstance()->getByAccount(account)) {
            result->setErrno(USER_ACCOUNT_EXISTS);
            break;
        }
        if (UserMgr::GetInstance()->getByEmail(email)) {
            result->setErrno(USER_EMAIL_EXISTS);
            break;
        }
        if (!IsEmail(email)) {
            result->setErrno(USER_INVALID_EMAIL);
            break;
        }
        if (!IsValidAccount(account)) {
            result->setErrno(USER_INVALID_ACCOUNT);
            break;
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_CONNECTION_FAILED);
            break;
        }

        // 根据系统设置决定是否需要校验邮箱验证码
        auto sys_settings = SystemSettingsMgr::GetInstance()->get();
        bool require_verification = true;
        if (sys_settings) {
            require_verification = sys_settings->getRequireEmailVerification() != 0;
        }
        if (require_verification) {
            if (!verificationEmailCode(email, auth_code)) {
                result->setErrno(AUTH_CODE_INVALID);
                break;
            }
        }

        std::string passwd_hash = HashPassword(passwd);
        if (passwd_hash.empty()) {
            result->setErrno(INTERNAL_ERROR);
            break;
        }

        // 开启事务
        chen::ITransaction::ptr trans = db->openTransaction();
        DB::UserInfo::ptr info(new DB::UserInfo);
        info->setAccount(account);
        info->setEmail(email);
        info->setPasswd(passwd_hash);
        info->setState(UserManager::Status::ACTIVE);
        info->setName(account);

        if (DB::UserInfoDao::Insert(info, db)) {
            result->setErrno(DB_OPERATION_FAILED, "insert user failed");
            break;
        }
        trans->commit();
        UserMgr::GetInstance()->add(info);
        INFO(logger) << info->toJsonString();
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

bool UserCreateServlet::verificationEmailCode(const std::string& email, const std::string& code)
{
    auto rpy = chen::RedisUtil::Cmd(GetRedisPoolName(), "GET email:verify:1:%s", email.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_STRING) {
        return false;
    }
    if (code != rpy->str) {
        return false;
    }
    // 一次性使用，校验通过后删除
    chen::RedisUtil::Cmd(GetRedisPoolName(), "DEL email:verify:1:%s", email.c_str());
    return true;
}
