#pragma once

#include "BlogServlet.hpp"

class BlogLoginedServlet : public BlogServlet {
public:
    BlogLoginedServlet(const std::string& name);

protected:
    bool handlePre(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
