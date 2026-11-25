/**
 * @file OrganizationLeaveServlet.hpp
 * @brief 成员主动退出组织
 * @author Christins
 * @date 2026-11-25
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class OrganizationLeaveServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationLeaveServlet> ptr;

    OrganizationLeaveServlet();

protected:
    int32_t handle(chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
