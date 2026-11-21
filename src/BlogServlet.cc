#include "BlogServlet.hpp"

#include "./auth/AuthService.hpp"
#include "./auth/SessionStore.hpp"
#include "./i18n/I18n.hpp"
#include "./metrics/MetricsService.hpp"
#include "./util.h"

BlogServlet::BlogServlet(const std::string& name)
    : Servlet(name)
{
}

void BlogServlet::setRoute(const std::string& route)
{
    const RouteMetrics& route_metrics = GetRouteMetrics(route);

    m_requests_metric = route_metrics.requests;
    m_errors_metric = route_metrics.errors;
    m_duration_metric = route_metrics.duration;
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

        TRACE(logger) << "path=" << request->getPath()
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

        ERROR(logger) << "servlet exception path=" << (request ? request->getPath() : "-") << " what=" << e.what();
        result->setErrno(INTERNAL_ERROR);

        if (response) {
            response->setBody(result->toJsonString());
        }
    } catch (...) {
        used = chen::TimeUtil::GetCurrentUs() - ts;

        ERROR(logger) << "servlet unknown exception path=" << (request ? request->getPath() : "-");
        result->setErrno(INTERNAL_ERROR);

        if (response) {
            response->setBody(result->toJsonString());
        }
    }

    const RouteMetrics route_metrics = {m_requests_metric, m_errors_metric, m_duration_metric};
    RecordRequest(route_metrics, used, result->getErrno());

    return 0;
}

bool BlogServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!request->getCookie(CookieKey::SESSION_KEY).empty()) {
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
    INFO(logger) << GetRemoteIP(request, session) << "\t" << request->getCookie(CookieKey::SESSION_KEY, "-") << "\t"
                 << getUserId(request) << "\t" << result->getErrno() << "\t" << result->getMessage() << "\t"
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
    std::string sid = request->getCookie(CookieKey::SESSION_KEY);
    if (!sid.empty()) {
        auto data = chen::http::SessionDataMgr::GetInstance()->get(sid);
        if (data) {
            int64_t uid = data->getData<int64_t>(SessionDataKey::USER_ID);
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
    data->setData(SessionDataKey::ANONYMOUS, 1);
    chen::http::SessionDataMgr::GetInstance()->add(data);
    AuthService::SetSessionCookie(response, data->getId(), 0);
    request->setCookie(CookieKey::SESSION_KEY, data->getId());
    return data;
}

bool BlogServlet::initLogin(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response)
{
    AuthContext ctx = AuthService::Resolve(request, response, true);
    return ctx.valid();
}