/**
 * @file User2FAResetServlet.hpp
 * @brief 重置2FA — 使用恢复码重置密钥（手机丢失场景）
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../SessionData.hpp"

class User2FAResetServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<User2FAResetServlet> ptr;
    User2FAResetServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
