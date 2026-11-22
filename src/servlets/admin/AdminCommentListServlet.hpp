#pragma once

#include "../../SessionData.hpp"
#include "../BlogAdminServlet.hpp"

class AdminCommentListServlet : public BlogAdminServlet {
public:
    AdminCommentListServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
