/**
 * @file User2FAConfirmServlet.hpp
 * @brief 确认2FA — 用户输入TOTP验证码完成绑定
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include "../../SessionData.hpp"

class User2FAConfirmServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2FAConfirmServlet> ptr;
    User2FAConfirmServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
