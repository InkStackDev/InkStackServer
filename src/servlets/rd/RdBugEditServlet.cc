#include "RdBugEditServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/BugService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"

RdBugEditServlet::RdBugEditServlet()
    : BlogLoginedServlet("RdBugEditServlet")
{
}

int32_t RdBugEditServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, title, "title");

        BugInput input;
        input.id = id;
        input.org_id = org_id;
        input.creator_id = viewer.user_id;
        input.title = title;
        input.description = request->getParam("description");

        std::string severity_str = request->getParam("severity");
        if (!severity_str.empty()) {
            int32_t s = StringToSeverity(severity_str);
            if (s >= 0) {
                input.severity = s;
            }
        }

        std::string priority_str = request->getParam("priority");
        if (!priority_str.empty()) {
            int32_t p = StringToPriority(priority_str);
            if (p >= 0) {
                input.priority = p;
            }
        }

        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            int32_t s = StringToBugStatus(status_str);
            if (s >= 0) {
                input.status = s;
            }
        }

        input.assignee_id = request->getParamAs<int64_t>("assignee_id", 0);
        input.requirement_id = request->getParamAs<int64_t>("related_requirement_id", 0);
        input.module = request->getParam("module");
        input.steps_to_reproduce = request->getParam("steps_to_reproduce");
        input.environment = request->getParam("environment");

        DB::BugInfo::ptr info;
        auto rt = BugService::Save(viewer, input, info);
        if (rt == BugService::SaveResult::NotFound) {
            result->setErrno(BUG_NOT_FOUND);
            break;
        }
        if (rt == BugService::SaveResult::NotMember) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        if (rt == BugService::SaveResult::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != BugService::SaveResult::Ok) {
            result->setErrno(RD_UPDATE_FAILED);
            break;
        }

        Json::Value data;
        BuildBugJson(data, info);
        result->setData(data);

        notifyAssignee(input.assignee_id, info);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

void RdBugEditServlet::notifyAssignee(int64_t assignee_id, DB::BugInfo::ptr bug)
{
    if (!assignee_id) {
        return;
    }

    EventRdAssignData data = {};
    data.type = "bug";
    data.assignee_id = assignee_id;
    data.item_id = bug->getId();
    data.item_title = bug->getTitle();
    data.creator_id = bug->getCreatorId();

    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_RD_ASSIGN, std::move(data));
}
