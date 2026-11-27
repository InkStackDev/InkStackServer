#include "OrganizationReposSyncServlet.hpp"

#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"

OrganizationReposSyncServlet::OrganizationReposSyncServlet()
    : BlogLoginedServlet("OrganizationReposSyncServlet")
{
}

int32_t OrganizationReposSyncServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, repo_id, "repo_id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::OrganizationReposInfo::ptr repo;
        auto rt = OrganizationRepoService::MarkSyncing(viewer, repo_id, repo);
        if (rt == OrganizationRepoService::Result::NotFound) {
            result->setErrno(ORG_REPO_NOT_FOUND);
            break;
        }
        if (rt == OrganizationRepoService::Result::NotMember) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt == OrganizationRepoService::Result::AccessDenied) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        // 设置状态为 syncing 后立即返回，异步调 GitHub API
        chen::Scheduler::GetThis()->schedule([repo_id]() {
            OrganizationRepoService::SyncFromGitHub(repo_id);
        });
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
