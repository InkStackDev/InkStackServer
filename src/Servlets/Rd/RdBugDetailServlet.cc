#include "RdBugDetailServlet.hpp"

#include "../../Service/BugService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdBugDetailServlet::RdBugDetailServlet()
    : BlogLoginedServlet("RdBugDetailServlet")
{
}

int32_t RdBugDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        if (!id) {
            result->setErrno(PARAM_MISSING, "id is required");
            break;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::BugInfo::ptr info;
        auto rt = BugService::GetDetail(viewer, id, info);
        if (rt == BugService::ViewResult::NotFound) {
            result->setErrno(BUG_NOT_FOUND);
            break;
        }
        if (rt == BugService::ViewResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != BugService::ViewResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value data;
        BuildBugJson(data, info);
        result->setData(data);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
