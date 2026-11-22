#include "BlogAdminServlet.hpp"

BlogAdminServlet::BlogAdminServlet(const std::string& name)
    : BlogLoginedServlet(name)
{
}

bool BlogAdminServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!BlogLoginedServlet::handlePre(request, response, session, result)) {
        return false;
    }

    Viewer viewer = getViewer(request);
    if (!viewer.valid()) {
        result->setErrno(ACCOUNT_INVALID);
        return false;
    }
    if (!viewer.is_platform_admin) {
        result->setErrno(ACCESS_DENIED);
        return false;
    }
    return true;
}
