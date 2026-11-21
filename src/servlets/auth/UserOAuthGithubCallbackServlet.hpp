/**
 * @file UserOAuthGithubCallbackServlet.hpp
 * @brief GitHub OAuth 回调 — 换取 GitHub 用户并签发本站登录态
 * @author Christins
 * @date 2026-10-11
 * @copyright Apache 2.0
 */
#pragma once

#include "../../SessionData.hpp"

class UserOAuthGithubCallbackServlet : public BlogServlet {
public:
    typedef std::shared_ptr<UserOAuthGithubCallbackServlet> ptr;
    UserOAuthGithubCallbackServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
