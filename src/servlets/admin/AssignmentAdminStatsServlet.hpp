#pragma once

#include "../BlogAdminServlet.hpp"

#include "../../SessionData.hpp"

class AssignmentAdminStatsServlet : public BlogAdminServlet {
public:
    typedef std::shared_ptr<AssignmentAdminStatsServlet> ptr;
    AssignmentAdminStatsServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
