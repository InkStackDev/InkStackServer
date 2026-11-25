#include "UserQueryServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

UserQueryServlet::UserQueryServlet()
    : BlogServlet("UserQueryServlet")
{
}

int32_t UserQueryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, user_ids, "user_ids");

        std::vector<int64_t> ids;
        for (auto& i : chen::StringUtil::Split(user_ids, ',')) {
            ids.push_back(chen::TypeUtil::Atoi(i));
        }

        std::vector<UserQueryService::Brief> users;
        UserQueryService::QueryBrief(ids, users);

        Json::Value data;
        for (const auto& user : users) {
            Json::Value v;
            v["id"] = user.id;
            v["name"] = user.name;
            data.append(v);
        }
        result->setData(data);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
