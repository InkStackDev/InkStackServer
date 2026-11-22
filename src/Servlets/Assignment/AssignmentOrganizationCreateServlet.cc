#include "AssignmentOrganizationCreateServlet.hpp"

#include "../../Event/EventDefine.hpp"
#include "../../Service/AssignmentService.hpp"
#include "../../Service/OrganizationService.hpp"
#include "../../Util/Util.hpp"

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

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        auto access = OrganizationService::CheckManageAccess(viewer, org_id);
        if (access != OrganizationService::AccessResult::Ok) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        OrganizationService::UserBrief operator_info;
        OrganizationService::GetUserProfile(viewer.user_id, operator_info);
        std::string oper_name = operator_info.name;

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

            std::vector<int64_t> member_ids;
            OrganizationService::ListApprovedMemberIds(org_id, viewer.user_id, member_ids);

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
