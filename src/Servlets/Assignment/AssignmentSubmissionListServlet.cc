#include "AssignmentSubmissionListServlet.hpp"

#include "../../Service/AssignmentService.hpp"
#include "../../Util/Util.hpp"

AssignmentSubmissionListServlet::AssignmentSubmissionListServlet()
    : BlogLoginedServlet("AssignmentSubmissionListServlet")
{
}

int32_t AssignmentSubmissionListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, assign_id, "assign_id");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        AssignmentService::SubmissionView submission;
        auto rt = AssignmentService::GetSubmission(assign_id, uid, submission);
        if (rt == AssignmentService::ViewResult::NotFound) {
            result->setErrno(ASSIGNMENT_NOT_FOUND);
            break;
        }
        if (rt != AssignmentService::ViewResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        if (submission.has_submission) {
            result->set("user_id", submission.user_id);
            result->set("user_avatar", submission.user_avatar);
            result->set("user_name", submission.user_name);
            result->set("submit_time", submission.submit_time);
            result->set("status", submission.status);

            Json::Value list;
            for (const auto& resource : submission.resources) {
                Json::Value item;
                item["id"] = resource.id;
                item["file_name"] = resource.name;
                item["file_path"] = resource.path;
                item["file_size"] = resource.size;
                list.append(item);
            }
            result->set("list", list);
            result->set("total", (int64_t)submission.resources.size());
        }
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
