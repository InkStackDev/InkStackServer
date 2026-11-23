#include "CleanupServlet.hpp"

#include "../../Service/DatabaseAdminService.hpp"
#include "../../Util/Util.hpp"

#include <ctime>

CleanupServlet::CleanupServlet()
    : BlogAdminServlet("CleanupServlet")
{
}

int32_t CleanupServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t now = time(0);
        int64_t deadline = now - 30 * 24 * 3600;
        int64_t backup_count = 0;
        int64_t export_count = 0;
        int64_t freed_bytes = 0;

        if (!DatabaseAdminService::Cleanup(deadline, backup_count, export_count, freed_bytes)) {
            result->setErrno(DB_CONNECTION_FAILED);
            break;
        }

        result->set("cleaned", backup_count + export_count);
        result->set("freedBytes", freed_bytes);
        result->setErrno(SUCCESS);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
