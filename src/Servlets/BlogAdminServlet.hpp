#pragma once

#include "BlogLoginedServlet.hpp"

/**
 * @brief 管理员专用 Servlet
 * @note 在登录校验基础上要求当前账号状态正常且角色为管理员
 */
class BlogAdminServlet : public BlogLoginedServlet {
public:
    BlogAdminServlet(const std::string& name);

protected:
    bool handlePre(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
