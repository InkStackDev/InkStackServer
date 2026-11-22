#include "RdTaskDetailServlet.hpp"

#include "../../Access/AccessPolicy.hpp"
#include "../../Access/AccessScopeResolver.hpp"
#include "../../Repository/TaskRepository.hpp"
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

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        auto info = TaskRepository::Get(id, db);
        if (!info || info->getIsDeleted()) {
            result->setErrno(TASK_NOT_FOUND);
            break;
        }

        OrgScope scope;
        if (!AccessScopeResolver::ResolveOrgs(scope, viewer, db)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (!AccessPolicy::CanViewTask(viewer, scope, info->getOrgId(), info->getCreatorId(), info->getAssigneeId())) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        Json::Value data;
        BuildTaskJson(data, info);
        result->set("data", data);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
