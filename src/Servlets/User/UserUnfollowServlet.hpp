#pragma once

#include "../../SessionData.hpp"
#include "../BlogLoginedServlet.hpp"

class UserUnfollowServlet : public BlogLoginedServlet {
public:
    UserUnfollowServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
