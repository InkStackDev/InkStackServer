#pragma once

#include "../../SessionData.hpp"

class AssignmentDeleteServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<AssignmentDeleteServlet> ptr;
    AssignmentDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
