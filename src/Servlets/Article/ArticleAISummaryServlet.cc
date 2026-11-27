#include "ArticleAISummaryServlet.hpp"

#include "../../Auth/AuthService.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../I18n/I18n.hpp"
#include "../../Manager/MessageConfigManager.hpp"
#include "../../Service/ArticleAIService.hpp"
#include "../../Util/Util.hpp"

#include <json/json.h>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

ArticleAISummaryServlet::ArticleAISummaryServlet()
    : chen::http::SSEServlet("ArticleAISummaryServlet")
{
}

int32_t ArticleAISummaryServlet::onConnect(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session)
{
    const std::string lang = I18n::ResolveLanguage(request);
    auto send_status = [&](const std::string& type, const std::string& message_id) {
        Json::Value msg;
        msg["type"] = type;
        msg["message_id"] = message_id;
        msg["message"] = MessageConfigMgr::GetInstance()->render(lang, message_id);
        session->sendEvent(chen::JsonUtil::ToString(msg));
    };

    AuthContext ctx = AuthService::Resolve(request, nullptr, false);
    if (!ctx.valid() || !ctx.user) {
        send_status("error", "ai.summary.error.not_login");
        return -1;
    }

    Viewer viewer;
    viewer.user_id = ctx.uid;
    viewer.system_role = ctx.user->getRole();
    viewer.is_platform_admin = (viewer.system_role == UserRole::ADMIN);

    std::string article_id_str = request->getParam("article_id");
    if (article_id_str.empty()) {
        send_status("error", "ai.summary.error.param_required");
        return -1;
    }

    int64_t article_id = 0;
    try {
        article_id = chen::TypeUtil::Atoi(article_id_str);
    } catch (...) {
        send_status("error", "ai.summary.error.invalid_article_id");
        return -1;
    }

    send_status("start", "ai.summary.start");

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
        send_status("done", "ai.summary.done");
        return 0;
    case ArticleAIService::Result::NotLogin:
        send_status("error", "ai.summary.error.not_login");
        break;
    case ArticleAIService::Result::ArticleNotFound:
        send_status("error", "ai.summary.error.article_not_found");
        break;
    case ArticleAIService::Result::ConfigNotFound:
        send_status("error", "ai.summary.error.config_not_found");
        break;
    case ArticleAIService::Result::UnsupportedProvider:
        send_status("error", "ai.summary.error.unsupported_provider");
        break;
    case ArticleAIService::Result::DbError:
        send_status("error", "ai.summary.error.database");
        break;
    case ArticleAIService::Result::ClientError:
    default:
        send_status("error", "ai.summary.error.service_failed");
        break;
    }

    ERROR(logger) << "article ai summary failed, article_id=" << article_id << " result=" << (int)rt;
    return -1;
}

int32_t ArticleAISummaryServlet::onClose(chen::http::HttpRequest::ptr request, chen::http::SSESession::ptr session)
{
    return 0;
}
