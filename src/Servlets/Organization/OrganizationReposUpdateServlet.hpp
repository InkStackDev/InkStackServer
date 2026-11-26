/**
 * @file OrganizationReposUpdateServlet.hpp
 * @brief 组织仓库编辑接口
 * @author Christins
 * @date 2026-11-26
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class OrganizationReposUpdateServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationReposUpdateServlet> ptr;
    OrganizationReposUpdateServlet();

protected:
    int32_t handle(chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
