/**
 * @file OrganizationRepoPrService.hpp
 * @brief 组织仓库 PR 同步用例（GitHub API）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include "DB/OrganizationRepoPrsInfo.hpp"
#include "DB/OrganizationReposInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

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

    enum class ListStatus {
        OK,
        REPO_NOT_FOUND,
        ORG_NOT_FOUND,
        NOT_MEMBER,
        ACCESS_DENIED,
        DB_ERROR,
    };

    enum class DeleteStatus {
        OK,
        PR_NOT_FOUND,
        REPO_NOT_FOUND,
        NOT_MEMBER,
        ACCESS_DENIED,
        DB_ERROR,
    };

    static bool GetPR(int64_t pr_id, DB::OrganizationRepoPrsInfo::ptr& out);

    static ListStatus List(const Viewer& viewer,
        int64_t repo_id,
        int64_t org_id,
        const std::string& state,
        uint64_t offset,
        uint64_t limit,
        std::vector<DB::OrganizationRepoPrsInfo::ptr>& out,
        int64_t& total);

    static DeleteStatus Remove(const Viewer& viewer, int64_t pr_id);

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
