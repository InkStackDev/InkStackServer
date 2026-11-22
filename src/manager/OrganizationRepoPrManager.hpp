/**
 * @file OrganizationRepoPrManager.hpp
 * @brief 组织仓库 PR 管理器
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationRepoPrsInfo.hpp"
#include "DB/OrganizationReposInfo.hpp"

#include <Chen/Util/Singleton.hpp>

#include "../Service/OrganizationRepoPrService.hpp"

// 前置声明 RPC 协议结构体，避免在 header 中引入 protocol 头文件
struct tagGithubPRInfo;
struct tagGithubPRReviewInfo;

class OrganizationRepoPrManager {
public:
    typedef std::shared_ptr<OrganizationRepoPrManager> ptr;

    using SyncPrStatus = OrganizationRepoPrService::SyncStatus;

    void add(DB::OrganizationRepoPrsInfo::ptr info);

    void del(int64_t id);

    void onTick();

    DB::OrganizationRepoPrsInfo::ptr get(int64_t id);

    DB::OrganizationRepoPrsInfo::ptr getByRepoAndPrId(int64_t repo_id, int32_t github_pr_id);

    int64_t listByRepoPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t repo_id,
        const std::string& state,
        uint64_t offset,
        uint64_t limit);

    int64_t getCountByRepo(int64_t repo_id);

    int64_t listByOrgPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t org_id,
        const std::string& state,
        uint64_t offset,
        uint64_t limit);

    int64_t listByUserPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t uid,
        const std::string& state,
        uint64_t offset,
        uint64_t limit);

    /// 从 GitHub API 同步某个仓库的 PR
    /// @param max_prs 最多同步的 PR 条数，0 表示全量同步
    static void SyncFromGitHub(int64_t repo_id, const std::string& repo_url, const std::string& token, int max_prs = 0);

    /// 从 GitHub API 同步单条 PR（按本地 PR 记录 id）
    /// @param pr_id 本地 organization_repo_prs 记录 id
    /// @param[out] out 同步成功时返回更新后的 PR 对象
    /// @return 同步结果状态
    static SyncPrStatus SyncPrFromGitHub(int64_t pr_id, DB::OrganizationRepoPrsInfo::ptr& out);

    static DB::OrganizationReposInfo::ptr FindRepoByOwnerAndName(const std::string& owner, const std::string& name);
};

typedef chen::Singleton<OrganizationRepoPrManager> OrganizationRepoPrMgr;
