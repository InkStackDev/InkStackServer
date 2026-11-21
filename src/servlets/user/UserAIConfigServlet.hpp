/**
 * @file UserAIConfigServlet.hpp
 * @brief 用户AI配置 — 保存与获取
 * @author Christins
 * @date 2026-06-09
 * @copyright Apache 2.0
 */
#pragma once

#include "../../SessionData.hpp"

class UserAIConfigServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<UserAIConfigServlet> ptr;
    UserAIConfigServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
