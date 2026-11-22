#pragma once

#include "../BlogAdminServlet.hpp"

#include "../../SessionData.hpp"

class OrganizationAdminListsServlet : public BlogAdminServlet {
public:
    typedef std::shared_ptr<OrganizationAdminListsServlet> ptr;
    OrganizationAdminListsServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
