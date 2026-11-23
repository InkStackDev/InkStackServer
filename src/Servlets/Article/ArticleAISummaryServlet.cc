#include "ArticleAISummaryServlet.hpp"

#include "../../Auth/AuthService.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../Service/ArticleAIService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

ArticleAISummaryServlet::ArticleAISummaryServlet()
    : chen::http::SSEServlet("ArticleAISummaryServlet")
{
}

int32_t ArticleAISummaryServlet::onConnect(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session)
{
    AuthContext ctx = AuthService::Resolve(request, nullptr, false);
    if (!ctx.valid() || !ctx.user) {
        session->sendEvent(R"({"type":"error","message":"not login"})");
        return -1;
    }

    Viewer viewer;
    viewer.user_id = ctx.uid;
    viewer.system_role = ctx.user->getRole();
    viewer.is_platform_admin = (viewer.system_role == UserRole::ADMIN);

    std::string article_id_str = request->getParam("article_id");
    if (article_id_str.empty()) {
        session->sendEvent(R"({"type":"error","message":"param article_id is required"})");
        return -1;
    }

    int64_t article_id = 0;
    try {
        article_id = chen::TypeUtil::Atoi(article_id_str);
    } catch (...) {
        session->sendEvent(R"({"type":"error","message":"invalid article_id"})");
        return -1;
    }

    SendSSEJson(session, "start", "message", "开始生成总结...");

    auto on_chunk = [session](const std::string& chunk) {
        SendSSEJson(session, "chunk", "content", chunk);
    };
    auto on_error = [session](const std::string& error) {
        SendSSEJson(session, "error", "message", error);
    };

    std::string full_text;
    auto rt = ArticleAIService::Summarize(viewer, article_id, on_chunk, on_error, full_text);
    switch (rt) {
    case ArticleAIService::Result::Ok:
        SendSSEJson(session, "done", "message", "总结生成完成");
        return 0;
    case ArticleAIService::Result::NotLogin:
        SendSSEJson(session, "error", "message", "not login");
        break;
    case ArticleAIService::Result::ArticleNotFound:
        SendSSEJson(session, "error", "message", "article not found");
        break;
    case ArticleAIService::Result::ConfigNotFound:
        SendSSEJson(session, "error", "message", "AI config not found");
        break;
    case ArticleAIService::Result::UnsupportedProvider:
        SendSSEJson(session, "error", "message", "unsupported AI provider");
        break;
    case ArticleAIService::Result::DbError:
        SendSSEJson(session, "error", "message", "database error");
        break;
    case ArticleAIService::Result::ClientError:
    default:
        SendSSEJson(session, "error", "message", "AI service call failed");
        break;
    }

    ERROR(logger) << "article ai summary failed, article_id=" << article_id << " result=" << (int)rt;
    return -1;
}

int32_t ArticleAISummaryServlet::onClose(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session)
{
    return 0;
}
