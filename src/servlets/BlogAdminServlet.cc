#include "BlogAdminServlet.hpp"

#include "../Repository/UserRepository.hpp"
#include "../Domain/UserTypes.hpp"

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

    int64_t uid = getUserId(request);
    if (!uid) {
        result->setErrno(NOT_LOGIN);
        return false;
    }

    auto user = UserRepository::Get(uid, GetDB());
    if (!user) {
        result->setErrno(USER_NOT_FOUND);
        return false;
    }

    if (user->getState() != UserState::ACTIVE || user->getIsDeleted()) {
        result->setErrno(ACCOUNT_INVALID);
        return false;
    }

    if (user->getRole() != UserRole::ADMIN) {
        result->setErrno(ACCESS_DENIED);
        return false;
    }

    return true;
}
