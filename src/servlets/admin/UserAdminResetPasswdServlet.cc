#include "UserAdminResetPasswdServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Auth/AuthService.hpp"
#include "../../Event/EventDefine.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

UserAdminResetPasswdServlet::UserAdminResetPasswdServlet()
    : BlogAdminServlet("UserAdminResetPasswdServlet")
{
}

int32_t UserAdminResetPasswdServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_STRING(result, passwd_f, "passwd_f");
        DEFINE_AND_CHECK_STRING(result, passwd_s, "passwd_s");

        if (passwd_f.empty() || passwd_s.empty()) {
            result->setErrno(PARAM_MISSING, "password required");
            break;
        }

        if (passwd_f != passwd_s) {
            result->setErrno(USER_PASSWORDS_DIFFER);
            break;
        }

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_CONNECTION_FAILED);
            break;
        }

        std::string passwd_hash = HashPassword(passwd_s);
        if (passwd_hash.empty()) {
            result->setErrno(INTERNAL_ERROR);
            break;
        }

        chen::ITransaction::ptr trans = db->openTransaction();
        if (!trans) {
            result->setErrno(DB_TRANSACTION_FAILED);
            break;
        }
        DB::UserInfo::ptr info = UserMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        info->setPasswd(passwd_hash);

        if (DB::UserInfoDao::Update(info, db)) {
            result->setErrno(DB_OPERATION_FAILED, "insert user failed");
            break;
        }
        trans->commit();
        UserMgr::GetInstance()->update(info);
        AuthService::RevokeAllSessions(info->getId());

        // Notify affected user
        {
            EventUserAdminData data = {};
            data.type = "password_reset";
            data.user_id = id;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_USER_ADMIN, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
