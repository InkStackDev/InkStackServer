#include "AdminFeedbackDeleteServlet.hpp"

#include "../../Include/Managers.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>
#include "../../Service/FeedbackService.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

AdminFeedbackDeleteServlet::AdminFeedbackDeleteServlet()
    : BlogAdminServlet("AdminFeedbackDeleteServlet")
{
}

int32_t AdminFeedbackDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {

        DEFINE_AND_CHECK_STRING(result, id_str, "id");

        int64_t id = 0;
        try {
            id = chen::TypeUtil::Atoi(id_str);
        } catch (...) {
            result->setErrno(PARAM_INVALID, "invalid id");
            break;
        }

        if (!FeedbackService::Remove(id)) {
            result->setErrno(FEEDBACK_NOT_FOUND);
            break;
        }

        result->setErrno(SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFeedbackDeleteServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}
