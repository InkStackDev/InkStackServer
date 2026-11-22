#include "BroadcastCloseServlet.hpp"

#include "../../Manager/NotificationManager.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Service/NotificationService.hpp"
#include "../../Util/Util.hpp"

#include <Chen/Log/Log.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

BroadcastCloseServlet::BroadcastCloseServlet()
    : BlogLoginedServlet("BroadcastCloseServlet")
{
}

int32_t BroadcastCloseServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        auto uinfo = UserMgr::GetInstance()->get(uid);
        if (!uinfo || uinfo->getRole() != UserManager::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        if (!NotificationService::CloseBroadcast(id)) {
            result->setErrno(NOTIFICATION_SEND_FAILED);
            break;
        }

        result->setErrno(SUCCESS);
        result->set("msg", "已关闭");
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
