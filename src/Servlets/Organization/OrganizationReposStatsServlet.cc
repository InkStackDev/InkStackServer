#include "OrganizationReposStatsServlet.hpp"

#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"

OrganizationReposStatsServlet::OrganizationReposStatsServlet()
    : BlogLoginedServlet("OrganizationReposStatsServlet")
{
}

int32_t OrganizationReposStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        int64_t total = 0;
        auto rt = OrganizationRepoService::CountByOrg(org_id, total);
        if (rt == OrganizationRepoService::Result::OrgNotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total_repos", total);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
