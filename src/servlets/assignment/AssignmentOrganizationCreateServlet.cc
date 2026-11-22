#include "AssignmentOrganizationCreateServlet.hpp"

#include "../../Permission.hpp"
#include "../../Event/EventDefine.hpp"
#include "../../Manager/UserManager.hpp"
#include "../../Service/AssignmentService.hpp"
#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"

AssignmentOrganizationCreateServlet::AssignmentOrganizationCreateServlet()
    : BlogLoginedServlet("AssignmentOrganizationCreateServlet")
{
}

int32_t AssignmentOrganizationCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, subject_name, "subject_name");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, end_time, "end_time");
        DEFINE_AND_CHECK_TYPE(result, int32_t, max_size, "max_size");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");
        DEFINE_AND_CHECK_TYPE(result, int32_t, priority, "priority");
        DEFINE_AND_CHECK_STRING(result, file_type, "file_type");
        DEFINE_AND_CHECK_STRING(result, description, "description");
        int64_t assign_id = request->getParamAs<int64_t>("assign_id", 0);

        // check operator permission
        auto uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        std::string oper_name = current_user->getName();
        int32_t system_role = current_user->getRole();
        auto org_rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
        if (!org_rel) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        int32_t org_role = org_rel->getRole();

        if (!CanManageMembers(system_role, org_role)) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        if (max_size > 96) {
            result->setErrno(ASSIGNMENT_MAX_SIZE_EXCEED);
            break;
        }

        AssignmentInput input;
        input.id = assign_id;
        input.name = name;
        input.subject_name = subject_name;
        input.description = description;
        input.file_type = file_type;
        input.deadline = (int64_t)end_time;
        input.max_size = max_size;
        input.status = status;
        input.priority = priority;

        bool created = false;
        DB::AssignmentInfo::ptr assign_info;
        auto rt = AssignmentService::CreateForOrganization(org_id, oper_name, input, assign_info, created);
        if (rt == AssignmentService::OrgCreateResult::NotFound) {
            result->setErrno(ASSIGNMENT_NOT_FOUND);
            break;
        }
        if (rt != AssignmentService::OrgCreateResult::Ok) {
            result->setErrno(ASSIGNMENT_UPDATE_FAILED);
            break;
        }

        if (created) {
            result->set("assigned_by", oper_name);
            // Notify org members about new assignment
            std::vector<int64_t> member_ids;
            std::vector<DB::OrganizationUserRelInfo::ptr> org_members;
            OrganizationUserRelRepository::ListByOrgPages(org_members, org_id, 0, 10000, -1, true, GetDB());
            for (auto& m : org_members) {
                if (m->getStatus() != OrgMemberStatus::APPROVED)
                    continue;
                if (m->getUserId() == uid)
                    continue;
                member_ids.push_back(m->getUserId());
            }

            EventAssignmentData data = {};
            data.type = "created";
            data.assignment_id = assign_info->getId();
            data.assignment_name = name;
            data.subject_name = subject_name;
            data.member_user_ids = member_ids;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_TYPE_ASSIGNMENT, std::move(data));
        }

        result->set("id", assign_info->getId());
        result->set("name", assign_info->getName());
        result->set("subject_name", assign_info->getSubjectName());
        result->set("end_time", assign_info->getDeadline());
        result->set("max_size", assign_info->getMaxSize());
        result->set("status", assign_info->getStatus());
        result->set("priority", assign_info->getPriority());
        result->set("file_type", assign_info->getFileType());
        result->set("description", assign_info->getDescription());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
