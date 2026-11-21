#pragma once

#include "../../SessionData.hpp"

class ExportCreateServlet : public BlogLoginedServlet {
public:
    ExportCreateServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
