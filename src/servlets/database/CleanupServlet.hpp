#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class CleanupServlet : public BlogLoginedServlet {
public:
    CleanupServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
