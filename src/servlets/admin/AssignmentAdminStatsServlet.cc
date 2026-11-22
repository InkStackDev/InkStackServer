#include "AssignmentAdminStatsServlet.hpp"

#include "../../Service/AdminAssignmentService.hpp"
#include "../../Util/Util.hpp"

AssignmentAdminStatsServlet::AssignmentAdminStatsServlet()
    : BlogAdminServlet("AssignmentAdminStatsServlet")
{
}

int32_t AssignmentAdminStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        AdminAssignmentService::Stats stats;
        if (!AdminAssignmentService::GetStats(stats)) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_assignments", stats.total);
        result->set("active_assignments", stats.active);
        result->set("closed_assignments", stats.closed);
        result->set("draft_assignments", stats.draft);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
