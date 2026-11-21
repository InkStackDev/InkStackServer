#pragma once

#include "../../SessionData.hpp"
#include "../BlogLoginedServlet.hpp"

class AdminCommentDeleteServlet : public BlogLoginedServlet {
public:
    AdminCommentDeleteServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
