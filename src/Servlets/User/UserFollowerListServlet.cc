#include "UserFollowerListServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

UserFollowerListServlet::UserFollowerListServlet()
    : BlogLoginedServlet("UserFollowerListServlet")
{
}

int32_t UserFollowerListServlet::handle(
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

        int64_t target_uid = request->getParamAs<int64_t>("user_id", uid);
        int32_t offset = request->getParamAs<int32_t>("offset", 0);
        int32_t size = request->getParamAs<int32_t>("size", 20);

        std::vector<UserQueryService::FollowUser> users;
        int64_t total = 0;
        if (!UserQueryService::ListFollowers(target_uid, offset, size, users, total)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value arr(Json::arrayValue);
        for (const auto& user : users) {
            Json::Value item;
            item["id"] = user.id;
            item["name"] = user.name;
            item["account"] = user.account;
            item["avatar"] = user.avatar;
            item["bio"] = user.bio;
            item["following_count"] = user.following_count;
            item["follower_count"] = user.follower_count;
            item["create_time"] = user.create_time;
            arr.append(item);
        }

        result->setErrno(SUCCESS);
        result->set("list", arr);
        result->set("total", total);
        result->set("offset", offset);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
