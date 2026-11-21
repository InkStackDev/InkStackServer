#include "BlogLoginedServlet.hpp"

BlogLoginedServlet::BlogLoginedServlet(const std::string& name)
    : BlogServlet(name)
{
}

bool BlogLoginedServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!request->getCookie(CookieKey::SESSION_KEY).empty()) {
        result->setErrno(NOT_LOGIN);
        return false;
    }

    if (!initLogin(request, response)) {
        result->setErrno(NOT_LOGIN);
        return false;
    }
    return true;
}
