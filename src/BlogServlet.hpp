#pragma once

#include <chen/http/servlet.h>
#include <chen/http/session_data.h>
#include <chen/metrics/metrics.h>

#include "SessionData.hpp"

class BlogServlet : public chen::http::Servlet {
public:
    BlogServlet(const std::string& name);

    void setRoute(const std::string& route);

    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session) override;

protected:
    virtual bool handlePre(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    virtual int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) = 0;

    virtual bool handlePost(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result);

    bool initLogin(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response);

    int64_t getUserId(chen::http::HttpRequest::ptr request);

private:
    chen::http::SessionData::ptr getSessionData(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response);

private:
    chen::metrics::Counter* m_requests_metric = nullptr;
    chen::metrics::Counter* m_errors_metric = nullptr;
    chen::metrics::Histogram* m_duration_metric = nullptr;
};