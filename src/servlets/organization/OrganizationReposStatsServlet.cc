#include "OrganizationReposStatsServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationRepoRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        // 检查组织是否存在
        auto org = OrganizationRepository::Get(org_id, GetDB());
        if (!org) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }

        int64_t total = OrganizationRepoRepository::CountByOrg(org_id, GetDB());

        result->set("total_repos", total);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
