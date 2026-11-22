#include "UserListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include <algorithm>

#include "../../Repository/UserRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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
        UserRepository::GetAllIds(ids, false, GetDB());

        std::sort(ids.begin(), ids.end(), std::less<>());

        Json::Value json_ids;
        for (size_t i = 0; i < ids.size(); ++i) {
            json_ids.append(ids[i]);
        }
        result->set("ids", json_ids);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
