#pragma once

#include "../../SessionData.hpp"

class UserSendCodeServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserSendCodeServlet> ptr;
    UserSendCodeServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
