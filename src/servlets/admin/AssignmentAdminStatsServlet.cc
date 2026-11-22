#include "AssignmentAdminStatsServlet.hpp"

#include "../../Repository/AssignmentQueryRepository.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

AssignmentAdminStatsServlet::AssignmentAdminStatsServlet()
    : BlogLoginedServlet("AssignmentAdminStatsServlet")
{
}

int32_t AssignmentAdminStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
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

        auto stats = AssignmentQueryRepository::GetStats();

        result->setErrno(SUCCESS);
        result->set("total_assignments", stats.total);
        result->set("active_assignments", stats.active);
        result->set("closed_assignments", stats.closed);
        result->set("draft_assignments", stats.draft);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
