#include "UserIsFollowingServlet.hpp"

#include "../../Util/Util.hpp"
#include "../../Service/UserFollowRelService.hpp"

UserIsFollowingServlet::UserIsFollowingServlet()
    : BlogLoginedServlet("UserIsFollowingServlet")
{
}

int32_t UserIsFollowingServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");

        bool following = UserFollowRelService::IsFollowing(uid, user_id);

        result->setErrno(SUCCESS);
        result->set("is_following", following);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
