#include "UserAdminResetPasswdServlet.hpp"

#include <utility>

#include "../../Auth/AuthService.hpp"
#include "../../Event/EventDefine.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"
#include "DB/UserInfo.hpp"

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

        DB::UserInfo::ptr info;
        auto rt = UserService::SetPassword(id, passwd_s, info);
        if (rt == UserService::SetPasswordResult::Ok) {
            AuthService::RevokeAllSessions(info->getId());

            EventUserAdminData data = {};
            data.type = "password_reset";
            data.user_id = id;
            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_USER_ADMIN, std::move(data));
        } else if (rt == UserService::SetPasswordResult::UserNotFound) {
            result->setErrno(USER_NOT_FOUND);
        } else if (rt == UserService::SetPasswordResult::InvalidPassword) {
            result->setErrno(USER_INVALID_PASSWORD);
        } else if (rt == UserService::SetPasswordResult::InternalError) {
            result->setErrno(INTERNAL_ERROR);
        } else {
            result->setErrno(DB_OPERATION_FAILED, "reset password failed");
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
