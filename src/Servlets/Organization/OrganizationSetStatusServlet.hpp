/**
 * @file OrganizationSetStatusServlet.hpp
 * @brief 组织所有者设置组织状态
 * @author Christins
 * @date 2026-11-25
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class OrganizationSetStatusServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationSetStatusServlet> ptr;

    OrganizationSetStatusServlet();

protected:
    int32_t handle(chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
