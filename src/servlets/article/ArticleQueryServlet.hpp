#pragma once

#include "../../SessionData.hpp"

class ArticleQueryServlet : public BlogServlet {
public:
    typedef std::shared_ptr<ArticleQueryServlet> ptr;
    ArticleQueryServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
