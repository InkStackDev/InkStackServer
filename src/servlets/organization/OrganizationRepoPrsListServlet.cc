/**
 * @file OrganizationRepoPrsListServlet.cc
 * @brief 组织仓库 PR 列表接口实现
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "OrganizationRepoPrsListServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationRepository.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Repository/OrganizationRepoRepository.hpp"
#include "../../Repository/OrganizationRepoPrRepository.hpp"
#include "../../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationRepoPrsListServlet::OrganizationRepoPrsListServlet()
    : BlogLoginedServlet("OrganizationRepoPrsListServlet")
{
}

int32_t OrganizationRepoPrsListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t repo_id = request->getParamAs<int64_t>("repo_id", 0);
        int64_t org_id = request->getParamAs<int64_t>("org_id", 0);
        uint64_t page = request->getParamAs<uint64_t>("page", 1);
        uint64_t page_size = request->getParamAs<uint64_t>("page_size", 20);
        std::string state = request->getParam("state");

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

        uint64_t offset = (page - 1) * page_size;
        std::vector<DB::OrganizationRepoPrsInfo::ptr> prs;
        int64_t total = 0;

        if (repo_id) {
            // 模式1: 按仓库查
            auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
            if (!repo) {
                result->setErrno(ORG_REPO_NOT_FOUND);
                break;
            }
            int64_t rid = repo->getOrgId();
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(rid, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED, "无权访问该组织");
                break;
            }
            total = OrganizationRepoPrRepository::ListByRepoPages(prs, repo_id, state, offset, page_size, GetDB());
        } else if (org_id) {
            // 模式2: 按组织查
            auto org = OrganizationRepository::Get(org_id, GetDB());
            if (!org) {
                result->setErrno(ORG_NOT_FOUND);
                break;
            }
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED, "无权访问该组织");
                break;
            }
            total = OrganizationRepoPrRepository::ListByOrgPages(prs, org_id, state, offset, page_size, GetDB());
        } else {
            // 模式3: 按用户所有组织查
            total = OrganizationRepoPrRepository::ListByUserPages(prs, uid, state, offset, page_size, GetDB());
        }

        result->set("total", total);
        result->set("page", page);
        result->set("page_size", page_size);
        Json::Value list;
        for (const auto& pr : prs) {
            Json::Value item;
            item["id"] = pr->getId();
            item["repo_id"] = pr->getRepoId();
            item["github_pr_id"] = pr->getGithubPrId();
            item["title"] = pr->getTitle();
            item["description"] = pr->getDescription();
            item["state"] = pr->getState();
            item["priority"] = pr->getPriority();
            item["author"] = pr->getAuthor();
            item["head_branch"] = pr->getHeadBranch();
            item["base_branch"] = pr->getBaseBranch();
            item["commit_sha"] = pr->getCommitSha().substr(0, 7);
            item["changed_files"] = pr->getChangedFiles();
            item["additions"] = pr->getAdditions();
            item["deletions"] = pr->getDeletions();
            item["reviewers"] = pr->getReviewers();
            item["review_status"] = pr->getReviewStatus();
            item["closed_at"] = pr->getClosedAt();
            item["merged_at"] = pr->getMergedAt();
            item["created_at"] = pr->getCreateTime();
            item["updated_at"] = pr->getUpdateTime();
            list.append(item);
        }
        result->set("list", list);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
