#include "AssignmentDetailServlet.hpp"

#include "../../Service/AssignmentService.hpp"
#include "../../Util/Util.hpp"

AssignmentDetailServlet::AssignmentDetailServlet()
    : BlogLoginedServlet("AssignmentDetailServlet")
{
}

int32_t AssignmentDetailServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        AssignmentService::DetailView detail;
        auto rt = AssignmentService::GetDetail(id, detail);
        if (rt == AssignmentService::ViewResult::NotFound) {
            result->setErrno(ARTICLE_INVALID_ID);
            break;
        }
        if (rt != AssignmentService::ViewResult::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("id", detail.id);
        result->set("name", detail.name);
        result->set("status", detail.status);
        result->set("priority", detail.priority);
        result->set("subject_name", detail.subject_name);
        result->set("description", detail.description);
        result->set("end_time", detail.deadline);
        result->set("file_size", detail.max_size);
        result->set("file_type", detail.file_type);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
