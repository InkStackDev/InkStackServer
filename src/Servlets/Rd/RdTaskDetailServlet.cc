#include "RdTaskDetailServlet.hpp"

#include "../../Service/TaskService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdTaskDetailServlet::RdTaskDetailServlet()
    : BlogLoginedServlet("RdTaskDetailServlet")
{
}

int32_t RdTaskDetailServlet::handle(
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

        DB::TaskInfo::ptr info;
        auto rt = TaskService::GetDetail(viewer, id, info);
        if (rt == TaskService::ViewResult::NotFound) {
            result->setErrno(TASK_NOT_FOUND);
            break;
        }
        if (rt == TaskService::ViewResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != TaskService::ViewResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        Json::Value data;
        BuildTaskJson(data, info);
        result->setData(data);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
