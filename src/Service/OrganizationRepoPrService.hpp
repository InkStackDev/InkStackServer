/**
 * @file OrganizationRepoPrService.hpp
 * @brief 组织仓库 PR 同步用例（GitHub API）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationRepoPrsInfo.hpp"
#include "DB/OrganizationReposInfo.hpp"

#include <cstdint>
#include <string>

class OrganizationRepoPrService {
public:
    /// 单条 PR 同步结果
    enum class SyncStatus {
        OK,
        PR_NOT_FOUND,
        REPO_NOT_FOUND,
        NO_TOKEN,
        GITHUB_NOT_FOUND,
        RATE_LIMITED,
        BUSY,
        HTTP_ERROR,
        DB_ERROR,
    };

    /**
     * @brief 同步所有配置了 Token 的仓库（后台任务，每仓库限制条数）
     */
    static void SyncAll();

    /**
     * @brief 从 GitHub API 同步某个仓库的 PR
     * @param max_prs 最多同步的 PR 条数，0 表示全量同步
     */
    static void SyncFromGitHub(int64_t repo_id, const std::string& repo_url, const std::string& token, int max_prs = 0);

    /**
     * @brief 从 GitHub API 同步单条 PR（按本地 PR 记录 id）
     */
    static SyncStatus SyncPrFromGitHub(int64_t pr_id, DB::OrganizationRepoPrsInfo::ptr& out);

    static DB::OrganizationReposInfo::ptr FindRepoByOwnerAndName(const std::string& owner, const std::string& name);
};
