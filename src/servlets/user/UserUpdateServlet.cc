#include "UserUpdateServlet.hpp"

#include "../../Auth/AuthService.hpp"
#include "../../Service/UserService.hpp"
#include "../../Util/Util.hpp"

UserUpdateServlet::UserUpdateServlet()
    : BlogServlet("UserUpdateServlet")
{
}

int32_t UserUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string name = request->getParam("name");
        std::string passwd = request->getParam("passwd");
        std::string bio = request->getParam("bio");
        std::string website = request->getParam("website");
        std::string github = request->getParam("github");
        std::string avatar = request->getParam("avatar");
        std::string old_passwd = request->getParam("old_passwd");

        if (name.empty() && passwd.empty() && bio.empty() && website.empty() && avatar.empty()) {
            result->setErrno(USER_NO_PARAM);
            break;
        }

        if (!passwd.empty() && passwd.length() < 6) {
            result->setErrno(USER_INVALID_PASSWORD);
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        UserService::ProfileUpdate update;
        update.name = name;
        update.passwd = passwd;
        update.bio = bio;
        update.website = website;
        update.github = github;
        update.avatar = avatar;
        update.old_passwd = old_passwd;

        bool password_changed = false;
        auto rt = UserService::UpdateProfile(uid, update, password_changed);
        if (rt == UserService::UpdateProfileResult::Ok) {
            result->setErrno(SUCCESS);
        } else if (rt == UserService::UpdateProfileResult::UserNotFound) {
            result->setErrno(ACCOUNT_INVALID);
        } else if (rt == UserService::UpdateProfileResult::OldPasswordRequired) {
            result->setErrno(PARAM_MISSING, "old password required");
        } else if (rt == UserService::UpdateProfileResult::OldPasswordWrong) {
            result->setErrno(USER_OLD_PASSWORD_WRONG);
        } else if (rt == UserService::UpdateProfileResult::InvalidPassword) {
            result->setErrno(USER_INVALID_PASSWORD);
        } else if (rt == UserService::UpdateProfileResult::InternalError) {
            result->setErrno(INTERNAL_ERROR);
        } else {
            result->setErrno(DB_OPERATION_FAILED, "update user failed");
            break;
        }

        if (password_changed) {
            // 密码变更后全端下线，要求重新登录
            AuthService::RevokeAllSessions(uid);
            AuthService::ClearSessionCookie(response);
            result->set("relogin", true);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
