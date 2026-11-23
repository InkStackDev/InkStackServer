#pragma once

#include "../../SessionData.hpp"
#include "../BlogLoginedServlet.hpp"

class UserAssignmentListServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<UserAssignmentListServlet> ptr;
    UserAssignmentListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
