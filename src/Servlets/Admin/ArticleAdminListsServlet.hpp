#pragma once

#include "../../SessionData.hpp"
#include "../BlogAdminServlet.hpp"

class ArticleAdminListsServlet : public BlogAdminServlet {
public:
    typedef std::shared_ptr<ArticleAdminListsServlet> ptr;

    ArticleAdminListsServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
