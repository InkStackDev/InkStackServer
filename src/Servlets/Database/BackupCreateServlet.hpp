#pragma once

#include "../BlogAdminServlet.hpp"

#include "../../SessionData.hpp"

class BackupCreateServlet : public BlogAdminServlet {
public:
    BackupCreateServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
