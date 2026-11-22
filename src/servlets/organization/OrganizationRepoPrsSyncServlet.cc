/**
 * @file OrganizationRepoPrsSyncServlet.cc
 * @brief 组织仓库 PR 同步接口实现（repo_id 全量 / id 单条）
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#include "OrganizationRepoPrsSyncServlet.hpp"

#include <Chen/Log/Log.hpp>

#include "../../Util/Util.hpp"
#include "../../Domain/OrganizationTypes.hpp"
#include "../../Repository/OrganizationUserRelRepository.hpp"
#include "../../Repository/OrganizationRepoRepository.hpp"
#include "../../Repository/OrganizationRepoPrRepository.hpp"
#include "../../Service/OrganizationRepoPrService.hpp"
#include "../../Repository/UserRepository.hpp"
#include "../../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

/// 将单条同步状态映射为业务错误码（OK 不处理）
static void MapSyncPrError(OrganizationRepoPrService::SyncStatus st, Result::ptr result)
{
    switch (st) {
    case OrganizationRepoPrService::SyncStatus::PR_NOT_FOUND:
        result->setErrno(ORG_PR_NOT_FOUND);
        break;
    case OrganizationRepoPrService::SyncStatus::REPO_NOT_FOUND:
        result->setErrno(ORG_REPO_NOT_FOUND);
        break;
    case OrganizationRepoPrService::SyncStatus::NO_TOKEN:
        result->setErrno(ORG_REPO_TOKEN_MISSING);
        break;
    case OrganizationRepoPrService::SyncStatus::GITHUB_NOT_FOUND:
        result->setErrno(GITHUB_PR_NOT_FOUND);
        break;
    case OrganizationRepoPrService::SyncStatus::RATE_LIMITED:
        result->setErrno(GITHUB_RATE_LIMITED);
        break;
    case OrganizationRepoPrService::SyncStatus::BUSY:
        result->setErrno(ORG_PR_SYNC_IN_PROGRESS);
        break;
    case OrganizationRepoPrService::SyncStatus::HTTP_ERROR:
        result->setErrno(GITHUB_API_FAILED);
        break;
    case OrganizationRepoPrService::SyncStatus::DB_ERROR:
        result->setErrno(DB_OPERATION_FAILED);
        break;
    case OrganizationRepoPrService::SyncStatus::OK:
    default:
        break;
    }
}

OrganizationRepoPrsSyncServlet::OrganizationRepoPrsSyncServlet()
    : BlogLoginedServlet("OrganizationRepoPrsSyncServlet")
{
}

int32_t OrganizationRepoPrsSyncServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t id = request->getParamAs<int64_t>("id", 0);
        int64_t repo_id = request->getParamAs<int64_t>("repo_id", 0);
        if (!id && !repo_id) {
            result->setErrno(PARAM_MISSING, "param id or repo_id is required");
            break;
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(NOT_LOGIN);
            break;
        }

        // 传 id 时定位到其所属仓库
        if (id) {
            auto pr = OrganizationRepoPrRepository::Get(id, GetDB());
            if (!pr) {
                result->setErrno(ORG_PR_NOT_FOUND);
                break;
            }
            repo_id = pr->getRepoId();
        }

        // 检查仓库是否存在
        auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
        if (!repo) {
            result->setErrno(ORG_REPO_NOT_FOUND);
            break;
        }
        int64_t org_id = repo->getOrgId();
        std::string url = repo->getUrl();
        std::string token = repo->getToken();

        // 权限检查：系统管理员或组织内研发主管及以上
        auto user = UserRepository::Get(uid, GetDB());
        if (!user) {
            result->setErrno(USER_NOT_FOUND);
            break;
        }
        int32_t system_role = user->getRole();
        if (system_role != UserRole::ADMIN) {
            auto rel = OrganizationUserRelRepository::GetByOrgAndUser(org_id, uid, GetDB());
            if (!rel || rel->getStatus() != OrgMemberStatus::APPROVED) {
                result->setErrno(ACCESS_DENIED);
                break;
            }
            int32_t org_role = rel->getRole();
            if (org_role != OrgRole::ORG_ADMIN && org_role != OrgRole::DEV_LEAD) {
                result->setErrno(ACCESS_DENIED, "仅研发主管及以上角色可同步 PR");
                break;
            }
        }

        if (id) {
            // 单条 PR 同步：同步执行，便于直接返回 GitHub 错误
            if (token.empty()) {
                result->setErrno(ORG_REPO_TOKEN_MISSING);
                break;
            }

            DB::OrganizationRepoPrsInfo::ptr updated;
            auto st = OrganizationRepoPrService::SyncPrFromGitHub(id, updated);
            if (st != OrganizationRepoPrService::SyncStatus::OK) {
                MapSyncPrError(st, result);
                break;
            }

            Json::Value item;
            item["id"] = updated->getId();
            item["repo_id"] = updated->getRepoId();
            item["github_pr_id"] = updated->getGithubPrId();
            item["title"] = updated->getTitle();
            item["description"] = updated->getDescription();
            item["state"] = updated->getState();
            item["priority"] = updated->getPriority();
            item["author"] = updated->getAuthor();
            item["head_branch"] = updated->getHeadBranch();
            item["base_branch"] = updated->getBaseBranch();
            item["commit_sha"] = updated->getCommitSha().substr(0, 7);
            item["changed_files"] = updated->getChangedFiles();
            item["additions"] = updated->getAdditions();
            item["deletions"] = updated->getDeletions();
            item["reviewers"] = updated->getReviewers();
            item["review_status"] = updated->getReviewStatus();
            item["closed_at"] = updated->getClosedAt();
            item["merged_at"] = updated->getMergedAt();
            item["created_at"] = updated->getCreateTime();
            item["updated_at"] = updated->getUpdateTime();
            result->setData(item);
            break;
        }

        // 异步调度全量同步
        chen::Scheduler::GetThis()->schedule([repo_id, url, token]() {
            OrganizationRepoPrService::SyncFromGitHub(repo_id, url, token);
        });
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
