#include "OrganizationReposListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Repository/OrganizationRepoRepository.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

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

        // 检查用户是否是该组织成员（任意角色即可查看仓库）
        auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
        if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
            result->setErrno(ACCESS_DENIED, "无权访问该组织");
            break;
        }

        uint64_t offset = (page - 1) * page_size;
        std::vector<DB::OrganizationReposInfo::ptr> repos;
        int64_t total = OrganizationRepoRepository::ListByOrgPages(repos, org_id, offset, page_size, GetDB());

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
