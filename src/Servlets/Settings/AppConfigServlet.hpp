/**
 * @file AppConfigServlet.hpp
 * @brief 客户端语言配置接口
 * @author Christins
 * @date 2026-11-26
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogServlet.hpp"

#include "../../SessionData.hpp"

class AppConfigServlet : public BlogServlet {
public:
    typedef std::shared_ptr<AppConfigServlet> ptr;
    AppConfigServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
