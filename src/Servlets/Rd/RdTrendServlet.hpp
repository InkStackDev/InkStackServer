/**
 * @file RdTrendServlet.hpp
 * @brief 研发平台趋势分析接口
 * @author Christins
 * @date 2026-06-25
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogLoginedServlet.hpp"

#include <json/json.h>

class RdTrendServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdTrendServlet> ptr;
    RdTrendServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
