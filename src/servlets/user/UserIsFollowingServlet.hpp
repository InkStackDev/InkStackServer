#pragma once

#include "../../SessionData.hpp"

class UserIsFollowingServlet : public BlogLoginedServlet {
public:
    UserIsFollowingServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
