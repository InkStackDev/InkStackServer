#include "UserMutualFollowingListServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Util/StringUtil.hpp>

UserMutualFollowingListServlet::UserMutualFollowingListServlet()
    : BlogLoginedServlet("UserMutualFollowingListServlet")
{
}

int32_t UserMutualFollowingListServlet::handle(
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

        std::string keyword = chen::StringUtil::Trim(request->getParam("keyword"));
        int32_t offset = request->getParamAs<int32_t>("offset", 0);
        int32_t size = request->getParamAs<int32_t>("size", 20);

        std::vector<UserQueryService::Brief> users;
        int64_t total = 0;
        if (!UserQueryService::ListMutual(uid, keyword, offset, size, users, total)) {
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
            arr.append(item);
        }

        result->setErrno(SUCCESS);
        result->set("list", arr);
        result->set("total", total);
        result->set("offset", offset);
        result->set("size", (int32_t)arr.size());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
