#include "UserListServlet.hpp"

#include "../../Service/UserQueryService.hpp"
#include "../../Util/Util.hpp"

UserListServlet::UserListServlet()
    : BlogAdminServlet("UserListServlet")
{
}

int32_t UserListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::vector<int64_t> ids;
        UserQueryService::ListAllIds(ids);

        Json::Value json_ids;
        for (auto id : ids) {
            json_ids.append(id);
        }
        result->set("ids", json_ids);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
