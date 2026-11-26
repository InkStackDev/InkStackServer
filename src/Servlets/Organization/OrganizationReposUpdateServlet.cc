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

        OrganizationRepoUpdateInput input;
        input.id = id;

        std::string value;
        if (request->hasParam("name", &value)) {
            if (value.empty()) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            if (value.size() > 128) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            input.name = value;
        }
        if (request->hasParam("description", &value)) {
            if (value.size() > 1024) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            input.description = value;
        }
        if (request->hasParam("url", &value)) {
            if (value.empty()) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            if (value.size() > 512) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            if (value.find("github.com") == std::string::npos) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            input.url = value;
        }
        if (request->hasParam("language", &value)) {
            if (value.size() > 64) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            input.language = value;
        }
        if (request->hasParam("sort_order")) {
            int32_t sort_order = 0;
            if (!request->checkGetParamAs("sort_order", sort_order)) {
                result->setErrno(PARAM_INVALID);
                break;
            }
            input.sort_order = sort_order;
        }

        if (!input.name && !input.description && !input.url && !input.language && !input.sort_order) {
            result->setErrno(PARAM_INVALID);
            break;
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
