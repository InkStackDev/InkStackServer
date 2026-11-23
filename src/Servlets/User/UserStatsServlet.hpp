#pragma once

#include "../../SessionData.hpp"
#include "../BlogLoginedServlet.hpp"

class UserStatsServlet : public BlogLoginedServlet {
public:
    UserStatsServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
