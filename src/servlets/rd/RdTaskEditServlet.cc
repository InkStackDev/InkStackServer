#include "RdTaskEditServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Manager/TaskManager.hpp"
#include "../../Permission.hpp"
#include "../../Service/TaskService.hpp"
#include "../../Util/Util.hpp"
#include "RdHelper.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

RdTaskEditServlet::RdTaskEditServlet()
    : BlogLoginedServlet("RdTaskEditServlet")
{
}

int32_t RdTaskEditServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
        if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        int32_t org_role = rel->getRole();

        if (id) {
            auto existing = TaskMgr::GetInstance()->get(id);
            if (!existing || existing->getOrgId() != org_id) {
                result->setErrno(TASK_NOT_FOUND);
                break;
            }
            if (!CanEditTask(org_role, uid, existing->getAssigneeId())) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
        } else if (!CanCreateTask(org_role)) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, title, "title");

        TaskInput input;
        input.id = id;
        input.org_id = org_id;
        input.creator_id = uid;
        input.title = title;
        input.description = request->getParam("description");

        std::string priority_str = request->getParam("priority");
        if (!priority_str.empty()) {
            int32_t p = StringToPriority(priority_str);
            if (p >= 0) {
                input.priority = p;
            }
        }

        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            int32_t s = StringToTaskStatus(status_str);
            if (s >= 0) {
                input.status = s;
            }
        }

        input.assignee_id = request->getParamAs<int64_t>("assignee_id", 0);
        input.requirement_id = request->getParamAs<int64_t>("requirement_id", 0);
        input.bug_id = request->getParamAs<int64_t>("bug_id", 0);
        input.deadline = request->getParamAs<int64_t>("deadline", 0);
        input.estimated_hours = request->getParamAs<int64_t>("estimated_hours", 0);

        DB::TaskInfo::ptr info;
        auto rt = TaskService::Save(input, info);
        if (rt == TaskService::SaveResult::NotFound) {
            result->setErrno(TASK_NOT_FOUND);
            break;
        }
        if (rt != TaskService::SaveResult::Ok) {
            result->setErrno(RD_UPDATE_FAILED);
            break;
        }

        Json::Value data;
        BuildTaskJson(data, info);
        result->setData(data);

        notifyAssignee(input.assignee_id, info);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

void RdTaskEditServlet::notifyAssignee(int64_t assignee_id, DB::TaskInfo::ptr task)
{
    if (!assignee_id) {
        return;
    }

    EventRdAssignData data = {};
    data.type = "task";
    data.assignee_id = assignee_id;
    data.item_id = task->getId();
    data.item_title = task->getTitle();
    data.creator_id = task->getCreatorId();

    chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_RD_ASSIGN, std::move(data));
}
