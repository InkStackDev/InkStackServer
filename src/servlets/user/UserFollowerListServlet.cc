#include "UserFollowerListServlet.hpp"

#include "../../Repository/UserFollowRelRepository.hpp"
#include "../../Repository/UserRepository.hpp"

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

        std::vector<DB::UserFollowRelInfo::ptr> rels;
        UserFollowRelRepository::ListFollowers(rels, target_uid, offset, size, GetDB());

        Json::Value arr(Json::arrayValue);
        for (auto& rel : rels) {
            auto user = UserRepository::Get(rel->getFollowerId(), GetDB());
            if (!user || user->getIsDeleted()) {
                continue;
            }
            int64_t follower_id = user->getId();
            Json::Value item;
            item["id"] = follower_id;
            item["name"] = user->getName();
            item["account"] = user->getAccount();
            item["avatar"] = user->getAvatar();
            item["bio"] = user->getBio();
            item["following_count"] = UserFollowRelRepository::CountFollowing(follower_id, GetDB());
            item["follower_count"] = UserFollowRelRepository::CountFollowers(follower_id, GetDB());
            item["create_time"] = rel->getCreateTime();
            arr.append(item);
        }

        int64_t total = UserFollowRelRepository::CountFollowers(target_uid, GetDB());

        result->setErrno(SUCCESS);
        result->set("list", arr);
        result->set("total", total);
        result->set("offset", offset);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
