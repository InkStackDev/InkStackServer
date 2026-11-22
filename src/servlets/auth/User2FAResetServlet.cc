#include "User2FAResetServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Manager/UserManager.hpp"
#include "../../Util/TotpUtil.hpp"
#include "../../Util/Util.hpp"
#include "DB/UserRecoveryCodeInfo.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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

        auto info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        if (!info->getTotpEnabled()) {
            result->setErrno(TOTP_NOT_ENABLED);
            break;
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        // 查找匹配且未使用的恢复码
        std::string codeHash = TotpUtil::HashRecoveryCode(recoveryCode);
        auto qb = DB::UserRecoveryCodeInfoDao::newQuery();
        qb->where("user_id", "=", uid);
        qb->where("code_hash", "=", codeHash);
        qb->where("used", "=", (int64_t)0);

        std::vector<DB::UserRecoveryCodeInfo::ptr> codes;
        if (DB::UserRecoveryCodeInfoDao::QueryByBuilder(codes, qb, db)) {
            result->setErrno(RECOVERY_CODE_INVALID);
            break;
        }

        if (codes.empty()) {
            result->setErrno(RECOVERY_CODE_INVALID);
            break;
        }

        // 标记恢复码为已使用
        auto matchedCode = codes[0];
        matchedCode->setUsed(1);
        DB::UserRecoveryCodeInfoDao::Update(matchedCode, db);

        // 禁用2FA
        info->setTotpEnabled(0);
        info->setTotpSecret("");
        info->setUpdateTime(time(0));

        if (DB::UserInfoDao::Update(info, db)) {
            result->setErrno(TOTP_ENABLE_FAILED, "reset 2fa failed");
            ERROR(logger) << "reset 2fa: Update failed"
                          << " errstr=" << db->getErrStr() << " errno=" << db->getErrno();
            break;
        }

        // 更新缓存
        UserMgr::GetInstance()->update(info);

        // 生成新密钥供重新绑定
        std::string newSecret = TotpUtil::GenerateSecret();
        std::string otpUri = TotpUtil::GetOtpUri(newSecret, info->getAccount());

        // 将新密钥临时存入session
        auto sdata = getSessionData(request, response);
        sdata->setData("totp_pending_secret", newSecret);

        result->set("secret", newSecret);
        result->set("otp_uri", otpUri);
        INFO(logger) << "user=" << uid << " reset 2fa via recovery code";
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
