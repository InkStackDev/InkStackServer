#pragma once

#include <Chen/Http/Servlet.hpp>
#include <Chen/Http/SessionData.hpp>
#include <Chen/Metrics/Metrics.hpp>

#include "../Domain/AccessTypes.hpp"
#include "../SessionData.hpp"

class BlogServlet : public chen::http::Servlet {
public:
    BlogServlet(const std::string& name);

    void setRoute(const std::string& route, const std::string& method);

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

    Viewer getViewer(chen::http::HttpRequest::ptr request);

protected:
    chen::http::SessionData::ptr getSessionData(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response);

private:
    chen::metrics::Counter* requests_metric_ = nullptr;
    chen::metrics::Counter* errors_metric_ = nullptr;
    chen::metrics::Histogram* duration_metric_ = nullptr;
};
