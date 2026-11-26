#include "OrganizationReposListServlet.hpp"

#include "../../Service/OrganizationRepoService.hpp"
#include "../../Util/Util.hpp"

OrganizationReposListServlet::OrganizationReposListServlet()
    : BlogLoginedServlet("OrganizationReposListServlet")
{
}

int32_t OrganizationReposListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");
        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 20);
        if (page_size > 50) {
            page_size = 50;
        }
        if (page < 1) {
            page = 1;
        }

        Viewer viewer = getViewer(request);
        if (!viewer.valid()) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        uint64_t offset = (page - 1) * page_size;
        std::vector<DB::OrganizationReposInfo::ptr> repos;
        int64_t total = 0;
        auto rt = OrganizationRepoService::List(viewer, org_id, offset, page_size, repos, total);
        if (rt == OrganizationRepoService::Result::OrgNotFound) {
            result->setErrno(ORG_NOT_FOUND);
            break;
        }
        if (rt == OrganizationRepoService::Result::NotMember) {
            result->setErrno(ACCESS_DENIED);
            break;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            result->setErrno(DB_OPERATION_FAILED);
            break;
        }

        result->set("total", total);
        result->set("page", page);
        result->set("page_size", page_size);
        Json::Value list;
        for (const auto& repo : repos) {
            Json::Value item;
            item["id"] = repo->getId();
            item["org_id"] = repo->getOrgId();
            item["name"] = repo->getName();
            item["description"] = repo->getDescription();
            item["url"] = repo->getUrl();
            item["language"] = repo->getLanguage();
            item["stars_count"] = repo->getStarsCount();
            item["sort_order"] = repo->getSortOrder();
            item["created_at"] = repo->getCreateTime();
            item["updated_at"] = repo->getUpdateTime();
            item["has_token"] = !repo->getToken().empty();
            item["sync_status"] = repo->getSyncStatus();
            item["pr_sync_status"] = repo->getPrSyncStatus();
            item["pr_synced_at"] = repo->getPrSyncedAt();
            list.append(item);
        }
        result->set("list", list);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}
