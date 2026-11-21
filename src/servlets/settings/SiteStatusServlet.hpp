#pragma once

#include "../../SessionData.hpp"

class SiteStatusServlet : public BlogServlet {
public:
    typedef std::shared_ptr<SiteStatusServlet> ptr;
    SiteStatusServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
