/**
 * @file User2FADisableServlet.hpp
 * @brief 禁用2FA — 验证TOTP码后禁用双因素认证
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class User2FADisableServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2FADisableServlet> ptr;
    User2FADisableServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
