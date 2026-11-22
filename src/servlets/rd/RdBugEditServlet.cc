#include "RdBugEditServlet.hpp"

#include "../../Permission.hpp"
#include "../../Event/EventDefine.hpp"
#include "../../Manager/BugManager.hpp"
#include "../../Manager/OrganizationUserRelManager.hpp"
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

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto rel = OrganizationUserRelMgr::GetInstance()->getByOrgAndUser(org_id, uid);
        if (!rel || rel->getStatus() != OrganizationUserRelManager::Status::APPROVED) {
            result->setErrno(ORG_NOT_MEMBER);
            break;
        }
        int32_t org_role = rel->getRole();

        DB::BugInfo::ptr info;
        if (id) {
            info = BugMgr::GetInstance()->get(id);
            if (!info || info->getOrgId() != org_id) {
                result->setErrno(BUG_NOT_FOUND);
                break;
            }
            if (!CanEditBug(org_role, uid, info->getCreatorId())) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
        } else {
            if (!CanCreateBug(org_role)) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            info.reset(new DB::BugInfo);
            info->setOrgId(org_id);
            info->setCreatorId(uid);
            info->setCreateTime(time(0));
        }

        DEFINE_AND_CHECK_STRING(result, title, "title");
        if (!title.empty()) {
            info->setTitle(title);
        }

        std::string desc = request->getParam("description");
        if (!desc.empty()) {
            info->setDescription(desc);
        }

        std::string severity_str = request->getParam("severity");
        if (!severity_str.empty()) {
            int32_t s = StringToSeverity(severity_str);
            if (s >= 0) {
                info->setSeverity(s);
            }
        }

        std::string priority_str = request->getParam("priority");
        if (!priority_str.empty()) {
            int32_t p = StringToPriority(priority_str);
            if (p >= 0) {
                info->setPriority(p);
            }
        }

        std::string status_str = request->getParam("status");
        if (!status_str.empty()) {
            int32_t s = StringToBugStatus(status_str);
            if (s >= 0) {
                info->setStatus(s);
            }
        }

        int64_t assignee_id = request->getParamAs<int64_t>("assignee_id", 0);
        if (assignee_id) {
            info->setAssigneeId(assignee_id);
        }

        int64_t requirement_id = request->getParamAs<int64_t>("related_requirement_id", 0);
        if (requirement_id) {
            info->setRequirementId(requirement_id);
        }

        std::string module = request->getParam("module");
        if (!module.empty()) {
            info->setModule(module);
        }

        std::string steps = request->getParam("steps_to_reproduce");
        if (!steps.empty()) {
            info->setStepsToReproduce(steps);
        }

        std::string env = request->getParam("environment");
        if (!env.empty()) {
            info->setEnvironment(env);
        }

        info->setIsDeleted(0);
        info->setUpdateTime(time(0));

        auto db = GetDB();
        if (!db) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }
        if (DB::BugInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(RD_UPDATE_FAILED);
            break;
        }
        Json::Value data;
        BuildBugJson(data, info);
        result->setData(data);

        notifyAssignee(assignee_id, info);
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
