#include "FeedbackServlet.hpp"

#include "../../Include/Managers.hpp"
#include "../../Service/FeedbackService.hpp"
#include "../../Util/Util.hpp"

FeedbackServlet::FeedbackServlet()
    : BlogServlet("FeedbackServlet")
{
}

int32_t FeedbackServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_STRING(result, content, "content");
        std::string contact = request->getParamAs<std::string>("contact");

        int64_t uid = getUserId(request);

        DB::UserFeedbackInfo::ptr info;
        if (!FeedbackService::Submit(uid, type, content, contact, info)) {
            result->setErrno(DB_OPERATION_FAILED, "feedback insert failed");
            break;
        }

        result->set("id", info->getId());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
