#pragma once

#include "../BlogAdminServlet.hpp"

#include "../../SessionData.hpp"

class DatabaseStatsServlet : public BlogAdminServlet {
public:
    DatabaseStatsServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
