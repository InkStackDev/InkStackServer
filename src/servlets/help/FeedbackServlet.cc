#include "FeedbackServlet.hpp"

#include "../../Include/Managers.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

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

        DB::UserFeedbackInfo::ptr info(new DB::UserFeedbackInfo);
        info->setType(type);
        info->setContent(content);
        info->setContact(contact);
        info->setUserId(uid);
        info->setCreateTime(time(0));
        info->setUpdateTime(time(0));

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (DB::UserFeedbackInfoDao::Insert(info, db)) {
            result->setErrno(DB_OPERATION_FAILED, "feedback insert failed");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        result->set("id", info->getId());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
