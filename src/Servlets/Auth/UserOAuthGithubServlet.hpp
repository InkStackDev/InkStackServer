/**
 * @file UserOAuthGithubServlet.hpp
 * @brief GitHub OAuth 登录入口 — 生成 state 并重定向到 GitHub 授权页
 * @author Christins
 * @date 2026-10-11
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogServlet.hpp"

#include "../../SessionData.hpp"

class UserOAuthGithubServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserOAuthGithubServlet> ptr;
    UserOAuthGithubServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
