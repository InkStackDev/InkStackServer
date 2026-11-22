/**
 * @file User2FAEnableServlet.hpp
 * @brief 启用2FA — 生成TOTP密钥和OTP URI
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class User2FAEnableServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2FAEnableServlet> ptr;
    User2FAEnableServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
