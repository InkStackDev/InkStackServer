/**
 * @file ArticleAISummaryServlet.hpp
 * @brief 文章AI总结 — SSE流式接口
 * @author Christins
 * @date 2026-06-10
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Http/SSEServlet.hpp>
#include <Chen/Http/SSESession.hpp>

class ArticleAISummaryServlet : public chen::http::SSEServlet {
public:
    typedef std::shared_ptr<ArticleAISummaryServlet> ptr;
    ArticleAISummaryServlet();

    virtual int32_t onConnect(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session) override;
    virtual int32_t onClose(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session) override;
};
