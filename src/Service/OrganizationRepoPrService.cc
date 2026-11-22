#include "OrganizationRepoPrService.hpp"

#include <ctime>
#include <string>
#include <vector>

#include <Chen/Http/Uri.hpp>
#include <Chen/IOManager/Worker.hpp>
#include <Chen/Log/Log.hpp>

#include "OrganizationRepoService.hpp"

#include "../Client/GitHubClient.hpp"
#include "../Job/RedisLease.hpp"
#include "../Repository/OrganizationRepoPrRepository.hpp"
#include "../Repository/OrganizationRepoRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

void OrganizationRepoPrService::SyncAll()
{
    std::vector<DB::OrganizationReposInfo::ptr> repos;
    OrganizationRepoRepository::ListWithToken(repos, GetDB());

    constexpr int kMaxPrCount = 15;
    for (const auto& repo : repos) {
        if (!repo) {
            continue;
        }
        SyncFromGitHub(repo->getId(), repo->getUrl(), repo->getToken(), kMaxPrCount);
    }
}

bool OrganizationRepoPrService::GetPR(int64_t pr_id, DB::OrganizationRepoPrsInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return false;
    }
    out = OrganizationRepoPrRepository::Get(pr_id, db);
    return out != nullptr;
}

OrganizationRepoPrService::ListStatus OrganizationRepoPrService::List(const Viewer& viewer,
    int64_t repo_id,
    int64_t org_id,
    const std::string& state,
    uint64_t offset,
    uint64_t limit,
    std::vector<DB::OrganizationRepoPrsInfo::ptr>& out,
    int64_t& total)
{
    out.clear();
    total = 0;
    if (!viewer.valid()) {
        return ListStatus::ACCESS_DENIED;
    }
    auto db = GetDB();
    if (!db) {
        return ListStatus::DB_ERROR;
    }

    if (repo_id) {
        DB::OrganizationReposInfo::ptr repo;
        auto rt = OrganizationRepoService::CheckRepoAccess(viewer, repo_id, false, repo);
        if (rt == OrganizationRepoService::Result::NotFound) {
            return ListStatus::REPO_NOT_FOUND;
        }
        if (rt == OrganizationRepoService::Result::NotMember) {
            return ListStatus::NOT_MEMBER;
        }
        if (rt == OrganizationRepoService::Result::AccessDenied) {
            return ListStatus::ACCESS_DENIED;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            return ListStatus::DB_ERROR;
        }
        total = OrganizationRepoPrRepository::ListByRepoPages(out, repo_id, state, offset, limit, db);
        return ListStatus::OK;
    }

    if (org_id) {
        auto rt = OrganizationRepoService::CheckOrgAccess(viewer, org_id, false);
        if (rt == OrganizationRepoService::Result::OrgNotFound) {
            return ListStatus::ORG_NOT_FOUND;
        }
        if (rt == OrganizationRepoService::Result::NotMember) {
            return ListStatus::NOT_MEMBER;
        }
        if (rt == OrganizationRepoService::Result::AccessDenied) {
            return ListStatus::ACCESS_DENIED;
        }
        if (rt != OrganizationRepoService::Result::Ok) {
            return ListStatus::DB_ERROR;
        }
        total = OrganizationRepoPrRepository::ListByOrgPages(out, org_id, state, offset, limit, db);
        return ListStatus::OK;
    }

    total = OrganizationRepoPrRepository::ListByUserPages(out, viewer.user_id, state, offset, limit, db);
    return ListStatus::OK;
}

OrganizationRepoPrService::DeleteStatus OrganizationRepoPrService::Remove(const Viewer& viewer, int64_t pr_id)
{
    if (!viewer.valid()) {
        return DeleteStatus::ACCESS_DENIED;
    }
    auto db = GetDB();
    if (!db) {
        return DeleteStatus::DB_ERROR;
    }
    auto pr = OrganizationRepoPrRepository::Get(pr_id, db);
    if (!pr) {
        return DeleteStatus::PR_NOT_FOUND;
    }

    DB::OrganizationReposInfo::ptr repo;
    auto rt = OrganizationRepoService::CheckRepoAccess(viewer, pr->getRepoId(), true, repo);
    if (rt == OrganizationRepoService::Result::NotFound) {
        return DeleteStatus::REPO_NOT_FOUND;
    }
    if (rt == OrganizationRepoService::Result::NotMember) {
        return DeleteStatus::NOT_MEMBER;
    }
    if (rt == OrganizationRepoService::Result::AccessDenied) {
        return DeleteStatus::ACCESS_DENIED;
    }
    if (rt != OrganizationRepoService::Result::Ok) {
        return DeleteStatus::DB_ERROR;
    }

    if (OrganizationRepoPrRepository::DeleteById(pr_id, db)) {
        return DeleteStatus::DB_ERROR;
    }
    OrganizationRepoPrRepository::Evict(pr_id);
    return DeleteStatus::OK;
}

/// 从 GitHub URL 提取 owner/repo
static std::string ExtractGithubRepo(const std::string& url)
{
    auto uri = chen::Uri::Create(url);
    if (!uri) {
        return "";
    }
    std::string path = uri->getPath();
    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }
    if (path.size() > 4 && path.substr(path.size() - 4) == ".git") {
        path = path.substr(0, path.size() - 4);
    }
    return path;
}

static int WritePullRequests(const std::vector<GitHubPullRequest>& items, int64_t repo_id, chen::IDB::ptr db)
{
    int synced = 0;
    for (const auto& pr : items) {
        auto existing = OrganizationRepoPrRepository::GetByRepoAndPrId(repo_id, pr.number, db);
        DB::OrganizationRepoPrsInfo::ptr info;
        if (existing) {
            info = existing;
        } else {
            info.reset(new DB::OrganizationRepoPrsInfo);
            info->setRepoId(repo_id);
            info->setGithubPrId(pr.number);
            info->setReviewStatus("pending");
            info->setReviewers("[]");
        }
        if (pr.created_at) {
            info->setCreateTime(pr.created_at);
        }

        info->setTitle(pr.title);
        info->setDescription(pr.description);
        if (pr.merged || pr.merged_at > 0) {
            info->setState("merged");
        } else {
            info->setState(pr.state);
        }
        info->setAuthor(pr.author);
        info->setHeadBranch(pr.head_branch);
        info->setBaseBranch(pr.base_branch);
        info->setCommitSha(pr.commit_sha);

        if (pr.has_detail) {
            info->setAdditions(pr.additions);
            info->setDeletions(pr.deletions);
            info->setChangedFiles(pr.changed_files);
        }
        if (pr.has_reviews) {
            info->setReviewStatus(pr.review_status);
            info->setReviewers(pr.reviewers);
        }

        info->setClosedAt(pr.closed_at);
        info->setMergedAt(pr.merged_at);
        info->setUpdateTime(time(0));

        if (existing) {
            if (!OrganizationRepoPrRepository::Update(info, db)) {
                ERROR(logger) << "SyncFromGitHub: Update failed for PR #" << info->getGithubPrId()
                              << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
                continue;
            }
        } else {
            if (!OrganizationRepoPrRepository::Insert(info, db)) {
                ERROR(logger) << "SyncFromGitHub: Insert failed for PR #" << info->getGithubPrId()
                              << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
                continue;
            }
        }
        OrganizationRepoPrRepository::Put(info);
        ++synced;
    }
    return synced;
}

void OrganizationRepoPrService::SyncFromGitHub(int64_t repo_id,
    const std::string& repo_url,
    const std::string& token,
    int max_prs)
{
    std::string repo_path = ExtractGithubRepo(repo_url);
    if (repo_path.empty()) {
        ERROR(logger) << "SyncFromGitHub: invalid repo url=" << repo_url;
        return;
    }

    {
        auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
        if (repo) {
            auto db2 = GetDB();
            if (db2) {
                repo->setPrSyncStatus("syncing");
                repo->setGithubFullName(repo_path);
                if (OrganizationRepoRepository::Update(repo, db2)) {
                    OrganizationRepoRepository::Put(repo);
                }
            }
        }
    }

    int per_page = max_prs > 0 ? max_prs : 0;
    int max_pages = max_prs > 0 ? 1 : 10;
    auto list = GitHubClient::ListPullRequests(token, repo_path, per_page, max_pages);
    if (!list.ok()) {
        ERROR(logger) << "SyncFromGitHub: list PR failed repo_id=" << repo_id << " error=" << (int)list.error
                      << " detail=" << list.detail;
        auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
        if (repo) {
            auto repoDb = GetDB();
            if (repoDb) {
                repo->setPrSyncStatus("failed");
                repo->setPrSyncedAt(time(0));
                if (OrganizationRepoRepository::Update(repo, repoDb)) {
                    OrganizationRepoRepository::Put(repo);
                }
            }
        }
        return;
    }

    // 并行补全详情与评审；单项失败保留列表字段，不影响整页写入。
    {
        auto wg = chen::WorkerGroup::Create(5);
        for (auto& pr : list.value) {
            wg->schedule([repo_path, token, &pr]() {
                auto detail = GitHubClient::GetPullRequest(token, repo_path, pr.number);
                if (detail.ok()) {
                    pr.additions = detail.value.additions;
                    pr.deletions = detail.value.deletions;
                    pr.changed_files = detail.value.changed_files;
                    pr.has_detail = true;
                }

                auto reviews = GitHubClient::GetReviewSummary(token, repo_path, pr.number);
                if (reviews.ok()) {
                    pr.review_status = reviews.value.status;
                    pr.reviewers = reviews.value.reviewers;
                    pr.has_reviews = true;
                } else if (reviews.error == ClientError::RateLimited) {
                    WARN(logger) << "SyncFromGitHub: reviews rate limited PR #" << pr.number;
                }
            });
        }
        wg->waitAll();
    }

    int total_synced = 0;
    {
        auto db = GetDB();
        if (!db) {
            ERROR(logger) << "SyncFromGitHub: GetDB failed before write";
            return;
        }
        total_synced = WritePullRequests(list.value, repo_id, db);
    }

    // 更新仓库的 PR 同步状态
    {
        auto repo = OrganizationRepoRepository::Get(repo_id, GetDB());
        if (repo) {
            auto repoDb = GetDB();
            if (repoDb) {
                repo->setPrSyncStatus("success");
                repo->setPrSyncedAt(time(0));
                if (OrganizationRepoRepository::Update(repo, repoDb)) {
                    OrganizationRepoRepository::Put(repo);
                }
            }
        }
    }

    INFO(logger) << "SyncFromGitHub: synced " << total_synced << " PRs for repo " << repo_id;
}

OrganizationRepoPrService::SyncStatus OrganizationRepoPrService::SyncPrFromGitHub(int64_t pr_id,
    DB::OrganizationRepoPrsInfo::ptr& out)
{
    auto pr = OrganizationRepoPrRepository::Get(pr_id, GetDB());
    if (!pr) {
        return SyncStatus::PR_NOT_FOUND;
    }

    auto repo = OrganizationRepoRepository::Get(pr->getRepoId(), GetDB());
    if (!repo) {
        return SyncStatus::REPO_NOT_FOUND;
    }

    std::string token = repo->getToken();
    if (token.empty()) {
        return SyncStatus::NO_TOKEN;
    }

    std::string repo_path = ExtractGithubRepo(repo->getUrl());
    if (repo_path.empty()) {
        ERROR(logger) << "SyncPrFromGitHub: invalid repo url=" << repo->getUrl();
        return SyncStatus::HTTP_ERROR;
    }

    int32_t number = pr->getGithubPrId();
    if (number <= 0) {
        ERROR(logger) << "SyncPrFromGitHub: invalid github_pr_id for pr_id=" << pr_id;
        return SyncStatus::HTTP_ERROR;
    }

    // 同一 PR 加锁，重复触发直接返回 BUSY
    RedisLease lease("sync:pr:" + std::to_string(pr_id), 300);
    if (!lease.TryAcquire()) {
        return SyncStatus::BUSY;
    }

    auto detail = GitHubClient::GetPullRequest(token, repo_path, number);
    if (!detail.ok()) {
        if (detail.error == ClientError::NotFound) {
            return SyncStatus::GITHUB_NOT_FOUND;
        }
        if (detail.error == ClientError::RateLimited) {
            return SyncStatus::RATE_LIMITED;
        }
        ERROR(logger) << "SyncPrFromGitHub: detail request failed pr_id=" << pr_id << " error=" << (int)detail.error;
        return SyncStatus::HTTP_ERROR;
    }

    const auto& item = detail.value;
    if (item.created_at) {
        pr->setCreateTime(item.created_at);
    }
    pr->setGithubPrId(item.number);
    pr->setTitle(item.title);
    pr->setDescription(item.description);
    pr->setAuthor(item.author);
    pr->setHeadBranch(item.head_branch);
    pr->setBaseBranch(item.base_branch);
    pr->setCommitSha(item.commit_sha);
    pr->setChangedFiles(item.changed_files);
    pr->setAdditions(item.additions);
    pr->setDeletions(item.deletions);
    if (item.merged || item.merged_at > 0) {
        pr->setState("merged");
    } else {
        pr->setState(item.state);
    }
    pr->setClosedAt(item.closed_at);
    pr->setMergedAt(item.merged_at);

    // 评审失败时保留原有评审字段，整体仍视为成功
    auto reviews = GitHubClient::GetReviewSummary(token, repo_path, number);
    if (reviews.ok()) {
        pr->setReviewStatus(reviews.value.status);
        pr->setReviewers(reviews.value.reviewers);
    } else if (reviews.error == ClientError::RateLimited) {
        WARN(logger) << "SyncPrFromGitHub: reviews rate limited pr_id=" << pr_id;
    }

    pr->setUpdateTime(time(0));

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncPrFromGitHub: GetDB failed pr_id=" << pr_id;
        return SyncStatus::DB_ERROR;
    }
    if (!OrganizationRepoPrRepository::Update(pr, db)) {
        ERROR(logger) << "SyncPrFromGitHub: Update failed for PR #" << pr->getGithubPrId()
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return SyncStatus::DB_ERROR;
    }
    OrganizationRepoPrRepository::Put(pr);
    out = pr;

    INFO(logger) << "SyncPrFromGitHub: synced pr_id=" << pr_id << " PR #" << pr->getGithubPrId()
                 << " state=" << pr->getState();
    return SyncStatus::OK;
}

DB::OrganizationReposInfo::ptr OrganizationRepoPrService::FindRepoByOwnerAndName(const std::string& owner,
    const std::string& name)
{
    if (owner.empty() || name.empty()) {
        return nullptr;
    }
    std::string full_name = owner + "/" + name;

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "findRepoByOwnerAndName: GetDB failed for " << full_name;
        return nullptr;
    }

    int64_t repoId = OrganizationRepoRepository::FindIdByGithubFullName(full_name, db);
    if (!repoId) {
        DEBUG(logger) << "findRepoByOwnerAndName: no repo found for " << full_name;
        return nullptr;
    }
    return OrganizationRepoRepository::Get(repoId, db);
}
