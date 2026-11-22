#include "AssignmentCreateServlet.hpp"

#include "../../Service/AssignmentService.hpp"
#include "../../Util/Util.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

AssignmentCreateServlet::AssignmentCreateServlet()
    : BlogLoginedServlet("AssignmentCreateServlet")
{
}

int32_t AssignmentCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, subject_name, "subject_name");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, end_time, "end_time");
        DEFINE_AND_CHECK_TYPE(result, int32_t, file_size, "file_size");
        DEFINE_AND_CHECK_TYPE(result, int32_t, priority, "priority");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");
        DEFINE_AND_CHECK_STRING(result, description, "description");
        DEFINE_AND_CHECK_STRING(result, file_type, "file_type");
        int64_t aid = request->getParamAs<int64_t>("id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }
        auto current_user = UserRepository::Get(uid, GetDB());
        if (!current_user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        if (current_user->getRole() != UserRole::ADMIN) {
            result->setErrno(ACCESS_DENIED);
            break;
        }

        if (file_size > 96) {
            result->setErrno(ASSIGNMENT_SIZE_EXCEED);
            break;
        }

        AssignmentInput input;
        input.id = aid;
        input.name = name;
        input.subject_name = subject_name;
        input.description = description;
        input.file_type = file_type;
        input.deadline = (int64_t)end_time;
        input.max_size = file_size;
        input.status = status;
        input.priority = priority;

        DB::AssignmentInfo::ptr info;
        auto rt = AssignmentService::CreateOrUpdate(input, info);
        if (rt == AssignmentService::CreateResult::NotFound) {
            result->setErrno(ASSIGNMENT_NOT_FOUND);
            break;
        }
        if (rt != AssignmentService::CreateResult::Ok) {
            result->setErrno(ASSIGNMENT_UPDATE_FAILED);
            break;
        }

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("subject_name", info->getSubjectName());
        result->set("status", info->getStatus());
        result->set("priority", info->getPriority());
        result->set("end_time", info->getDeadline());
        result->set("create_time", info->getCreateTime());
        result->set("file_size", info->getMaxSize());
        result->set("file_type", info->getFileType());
        result->set("description", info->getDescription());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
