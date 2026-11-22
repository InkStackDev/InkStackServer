#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class DashboardRecentUsersServlet : public BlogLoginedServlet {
public:
    DashboardRecentUsersServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
