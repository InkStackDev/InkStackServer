#pragma once

#include "../../SessionData.hpp"
#include "../BlogLoginedServlet.hpp"

class UserFollowServlet : public BlogLoginedServlet {
public:
    UserFollowServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
