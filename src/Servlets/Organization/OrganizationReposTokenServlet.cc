#include "OrganizationReposTokenServlet.hpp"

#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"

OrganizationReposTokenServlet::OrganizationReposTokenServlet()
    : BlogLoginedServlet("OrganizationReposTokenServlet")
{
}

int32_t OrganizationReposTokenServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, repo_id, "repo_id");
        DEFINE_AND_CHECK_STRING(result, token, "token");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DB::OrganizationReposInfo::ptr repo;
        auto rt = OrganizationRepoService::UpdateToken(viewer, repo_id, token, repo);
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
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
