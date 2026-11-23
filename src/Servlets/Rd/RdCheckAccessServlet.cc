#include "RdCheckAccessServlet.hpp"

#include "../../Service/RdAccessService.hpp"
#include "../../Util/Util.hpp"

RdCheckAccessServlet::RdCheckAccessServlet()
    : BlogLoginedServlet("RdCheckAccessServlet")
{
}

int32_t RdCheckAccessServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(RD_INVALID_USER);
            break;
        }

        int64_t target_uid = viewer.user_id;
        std::string uid_param = request->getParam("user_id");
        if (!uid_param.empty()) {
            try {
                target_uid = chen::TypeUtil::Atoi(uid_param);
            } catch (...) {
                result->setErrno(PARAM_INVALID, "invalid user_id");
                break;
            }
        }

        RdAccessService::CheckResult check;
        auto rt = RdAccessService::CheckPlatformAccess(viewer, target_uid, check);
        if (rt == RdAccessService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != RdAccessService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->setErrno(SUCCESS);
        result->set("can_access", check.can_access ? "1" : "0");
        if (!check.can_access) {
            result->set("reason", check.reason);
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
