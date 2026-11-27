#include "OrganizationReposUpdateServlet.hpp"

#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"

OrganizationReposUpdateServlet::OrganizationReposUpdateServlet()
    : BlogLoginedServlet("OrganizationReposUpdateServlet")
{
}

int32_t OrganizationReposUpdateServlet::handle(chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, url, "url");
        if (url.size() > 512 || url.find("github.com") == std::string::npos) {
            result->setErrno(PARAM_INVALID);
            break;
        }

        OrganizationRepoUpdateInput input;
        input.id = id;
        input.url = url;

        std::string token;
        if (request->hasParam("token", &token)) {
            input.token = token;
        }

        DB::OrganizationReposInfo::ptr info;
        auto rt = OrganizationRepoService::Update(viewer, input, info);
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
        if (rt == OrganizationRepoService::Result::NameExists) {
            result->setErrno(ORG_REPO_NAME_EXISTS);
            break;
        }
        if (rt == OrganizationRepoService::Result::InvalidParam) {
            result->setErrno(PARAM_INVALID);
            break;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("id", info->getId());
        result->set("org_id", info->getOrgId());
        result->set("name", info->getName());
        result->set("description", info->getDescription());
        result->set("url", info->getUrl());
        result->set("language", info->getLanguage());
        result->set("stars_count", info->getStarsCount());
        result->set("sort_order", info->getSortOrder());
        result->set("has_token", !info->getToken().empty());
        result->set("sync_status", info->getSyncStatus());
        result->set("pr_sync_status", info->getPrSyncStatus());
        result->set("pr_synced_at", info->getPrSyncedAt());
        result->set("created_at", info->getCreateTime());
        result->set("updated_at", info->getUpdateTime());
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
