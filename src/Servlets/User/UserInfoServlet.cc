#include "UserInfoServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

UserInfoServlet::UserInfoServlet()
    : BlogServlet("UserInfoServlet")
{
}

int32_t UserInfoServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t viewer_uid = getUserId(request);
        if (!viewer_uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        int64_t target_uid = viewer_uid;
        int64_t id = request->getParamAs<int64_t>("user_id", -1);
        if (id != -1) {
            target_uid = id;
        }

        UserQueryService::Profile profile;
        if (!UserQueryService::GetProfile(viewer_uid, target_uid, profile)) {
            result->setErrno(ACCOUNT_INVALID);
            break;
        }

        result->setErrno(SUCCESS);
        result->set("id", profile.id);
        result->set("name", profile.name);
        result->set("account", profile.account);
        result->set("avatar", profile.avatar);
        if (profile.include_email) {
            result->set("email", profile.email);
        }
        result->set("role", profile.role);
        result->set("bio", profile.bio);
        result->set("website", profile.website);
        result->set("location", profile.location);
        result->set("status", profile.status);
        result->set("github", profile.github);
        result->set("login_time", profile.login_time);
        result->set("create_time", profile.create_time);
        result->set("following_count", profile.following_count);
        result->set("follower_count", profile.follower_count);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
