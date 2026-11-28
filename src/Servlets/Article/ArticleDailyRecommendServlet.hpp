/**
 * @file ArticleDailyRecommendServlet.hpp
 * @brief 每日推荐接口 - 返回当天推荐榜单，登录用户在全局榜基础上个性化重排
 * @author Christins
 * @date 2026-11-28
 * @copyright Apache 2.0
 */
#pragma once

#include "../BlogServlet.hpp"

#include "../../SessionData.hpp"

class ArticleDailyRecommendServlet : public BlogServlet {
public:
    typedef std::shared_ptr<ArticleDailyRecommendServlet> ptr;
    ArticleDailyRecommendServlet();

protected:
    int32_t handle(chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};
