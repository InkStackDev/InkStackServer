#include "AdminFeedbackDeleteServlet.hpp"

#include "../../Include/Managers.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

AdminFeedbackDeleteServlet::AdminFeedbackDeleteServlet()
    : BlogLoginedServlet("AdminFeedbackDeleteServlet")
{
}

int32_t AdminFeedbackDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }

        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, id_str, "id");

        int64_t id = 0;
        try {
            id = chen::TypeUtil::Atoi(id_str);
        } catch (...) {
            result->setErrno(PARAM_INVALID, "invalid id");
            break;
        }

        if (!FeedbackMgr::GetInstance()->remove(id)) {
            result->setErrno(FEEDBACK_NOT_FOUND);
            break;
        }

        result->setErrno(SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFeedbackDeleteServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}
