#include "AdminFeedbackListServlet.hpp"

#include "../../Include/Managers.hpp"

#include <Chen/Log/Log.hpp>
#include "../../Service/FeedbackService.hpp"
#include "../../Service/UserQueryService.hpp"
#include "../../Domain/UserTypes.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

AdminFeedbackListServlet::AdminFeedbackListServlet()
    : BlogAdminServlet("AdminFeedbackListServlet")
{
}

int32_t AdminFeedbackListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {

        std::string type = request->getParam("type");
        int32_t page = request->getParamAs<int32_t>("page", 1);
        int32_t page_size = request->getParamAs<int32_t>("page_size", 20);

        std::vector<DB::UserFeedbackInfo::ptr> infos;
        int64_t total = 0;

        if (!FeedbackService::List(infos, type, page, page_size, total)) {
            result->setErrno(DB_OPERATION_FAILED, "query feedbacks failed");
            break;
        }

        Json::Value list(Json::arrayValue);
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = std::to_string(i->getId());
            v["type"] = i->getType();
            v["content"] = i->getContent();
            v["contact"] = i->getContact();
            v["created_at"] = static_cast<int64_t>(i->getCreateTime() * 1000);

            int64_t user_id = i->getUserId();
            v["user_id"] = user_id;
            UserQueryService::Brief user;
            UserQueryService::GetUserBrief(user_id, user);
            v["user_name"] = user.name;
            v["user_avatar"] = user.avatar;

            list.append(v);
        }

        result->set("total", total);
        result->set("list", list);
        result->setErrno(SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFeedbackListServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}
