#include "UserFollowServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>
#include "../../Service/UserFollowRelService.hpp"

UserFollowServlet::UserFollowServlet()
    : BlogLoginedServlet("UserFollowServlet")
{
}

int32_t UserFollowServlet::handle(
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

        DEFINE_AND_CHECK_TYPE(result, int64_t, following_id, "following_id");

        if (following_id == uid) {
            result->setErrno(USER_CANNOT_FOLLOW_SELF);
            break;
        }

        // check the target user exists
        auto targetUser = UserMgr::GetInstance()->get(following_id);
        if (!targetUser || targetUser->getIsDeleted()) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        bool already_following = UserFollowRelService::IsFollowing(uid, following_id);

        auto info = UserFollowRelService::Follow(uid, following_id);
        if (!info) {
            result->setErrno(USER_FOLLOW_FAILED);
            break;
        }

        // Notify the followed user (only for new follows)
        if (!already_following) {
            auto follower_info = UserMgr::GetInstance()->get(uid);
            EventUserFollowData data = {};
            data.follower_id = uid;
            data.follower_name = follower_info ? follower_info->getName() : "someone";
            data.following_id = following_id;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_USER_FOLLOW, std::move(data));
        }

        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
