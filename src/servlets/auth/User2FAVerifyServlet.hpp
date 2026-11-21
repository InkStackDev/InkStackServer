/**
 * @file User2FAVerifyServlet.hpp
 * @brief 2FA登录验证 — 第二步提交TOTP码完成登录
 * @author Christins
 * @date 2026-09-14
 * @copyright Apache 2.0
 */
#pragma once

#include "../../SessionData.hpp"

namespace blog {
namespace servlet {

class User2FAVerifyServlet : public BlogServlet {
public:
    typedef std::shared_ptr<User2FAVerifyServlet> ptr;
    User2FAVerifyServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
