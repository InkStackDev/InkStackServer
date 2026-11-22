#pragma once

#include "../BlogServlet.hpp"

#include "../../SessionData.hpp"

class CommentRepliesServlet : public BlogServlet {
public:
    CommentRepliesServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
