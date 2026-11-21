#include "BlogServlet.hpp"

#include "./Auth/AuthService.hpp"
#include "./Auth/SessionStore.hpp"
#include "./I18n/I18n.hpp"
#include "./Metrics/MetricsService.hpp"
#include "Util/Util.hpp"

BlogServlet::BlogServlet(const std::string& name)
    : Servlet(name)
{
}

void BlogServlet::setRoute(const std::string& route)
{
    const RouteMetrics& route_metrics = GetRouteMetrics(route);

    requests_metric_ = route_metrics.requests;
    errors_metric_ = route_metrics.errors;
    duration_metric_ = route_metrics.duration;
}

int32_t BlogServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session)
{
    Result::ptr result = std::make_shared<Result>();
    uint64_t ts = chen::TimeUtil::GetCurrentUs();
    uint64_t used = 0;
    try {
        result->setLanguage(I18n::ResolveLanguage(request));

        TRACE(g_session_logger) << "path=" << request->getPath()
                                << " - method=" << chen::http::HttpMethodToString(request->getMethod());

        if (handlePre(request, response, session, result)) {
            handle(request, response, session, result);
        } else {
            response->setBody(result->toJsonString());
        }

        used = chen::TimeUtil::GetCurrentUs() - ts;
        handlePost(request, response, session, result);
        response->setHeader("used", std::to_string((used * 1.0 / 1000)) + "ms");
    } catch (const std::exception& e) {
        used = chen::TimeUtil::GetCurrentUs() - ts;

        ERROR(g_session_logger) << "servlet exception path=" << (request ? request->getPath() : "-")
                                << " what=" << e.what();
        result->setErrno(INTERNAL_ERROR);

        if (response) {
            response->setBody(result->toJsonString());
        }
    } catch (...) {
        used = chen::TimeUtil::GetCurrentUs() - ts;

        ERROR(g_session_logger) << "servlet unknown exception path=" << (request ? request->getPath() : "-");
        result->setErrno(INTERNAL_ERROR);

        if (response) {
            response->setBody(result->toJsonString());
        }
    }

    const RouteMetrics route_metrics = {requests_metric_, errors_metric_, duration_metric_};
    RecordRequest(route_metrics, used, result->getErrno());

    return 0;
}

bool BlogServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!request->getCookie(CookieKey::kSessionKey).empty()) {
        initLogin(request, response);
    }
    return true;
}

bool BlogServlet::handlePost(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    std::string val;
    if (!response->checkGetHeaderAs<std::string>("Content-Type", val, "application/json")) {
        response->setHeader("Content-Type", val);
    }
    INFO(g_session_logger) << GetRemoteIP(request, session) << "\t" << request->getCookie(CookieKey::kSessionKey, "-")
                           << "\t" << getUserId(request) << "\t" << result->getErrno() << "\t" << result->getMessage() << "\t"
                 << request->getPath() << "\t" << (!request->getQuery().empty() ? request->getQuery() : "-");
    return true;
}

int64_t BlogServlet::getUserId(chen::http::HttpRequest::ptr request)
{
    AuthContext ctx = AuthService::Resolve(request, nullptr, false);
    return ctx.valid() ? ctx.uid : 0;
}

chen::http::SessionData::ptr BlogServlet::getSessionData(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response)
{
    std::string sid = request->getCookie(CookieKey::kSessionKey);
    if (!sid.empty()) {
        auto data = chen::http::SessionDataMgr::GetInstance()->get(sid);
        if (data) {
            int64_t uid = data->getData<int64_t>(SessionDataKey::kUserId);
            if (!uid) {
                // 匿名会话不参与认证
                return data;
            }
            // 已登录会话必须以 SessionStore 为准，避免撤销后内存中仍残留
            SessionInfo info;
            if (SessionStore::Get(sid, info)) {
                return data;
            }
            chen::http::SessionDataMgr::GetInstance()->del(sid);
        }

        SessionInfo info;
        if (SessionStore::Get(sid, info)) {
            AuthService::MirrorSessionData(sid, info.uid);
            auto session_data = chen::http::SessionDataMgr::GetInstance()->get(sid);
            if (session_data) {
                return session_data;
            }
        }
    }

    // 没有就创建一个匿名会话
    chen::http::SessionData::ptr data(new chen::http::SessionData(true));
    data->setData(SessionDataKey::kAnonymous, 1);
    chen::http::SessionDataMgr::GetInstance()->add(data);
    AuthService::SetSessionCookie(response, data->getId(), 0);
    request->setCookie(CookieKey::kSessionKey, data->getId());
    return data;
}

bool BlogServlet::initLogin(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response)
{
    AuthContext ctx = AuthService::Resolve(request, response, true);
    return ctx.valid();
}
