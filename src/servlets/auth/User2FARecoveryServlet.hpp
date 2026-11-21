/**
 * @file User2FARecoveryServlet.hpp
 * @brief 恢复码登录 — 2FA登录时的备用路径
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../SessionData.hpp"

class User2FARecoveryServlet : public BlogServlet {
public:
    typedef std::shared_ptr<User2FARecoveryServlet> ptr;
    User2FARecoveryServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
