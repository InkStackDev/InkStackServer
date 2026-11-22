/**
 * @file OrganizationRepoService.hpp
 * @brief 组织仓库用例：增删、Token、GitHub 元数据同步
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationReposInfo.hpp"

#include <cstdint>
#include <string>

struct OrganizationRepoInput {
    int64_t org_id = 0;
    std::string name;
    std::string url;
    std::string language;
    std::string description;
    std::string token;
};

class OrganizationRepoService {
public:
    enum class Result {
        Ok,
        NotFound,
        NameExists,
        DbError
    };

    static Result Add(const OrganizationRepoInput& input, DB::OrganizationReposInfo::ptr& out);

    static bool Remove(int64_t id);

    static Result UpdateToken(int64_t id, const std::string& token, DB::OrganizationReposInfo::ptr& out);

    static Result MarkSyncing(int64_t id, DB::OrganizationReposInfo::ptr& out);

    /**
     * @brief 从 GitHub API 同步单个仓库的 stars/language/description
     */
    static void SyncFromGitHub(int64_t repo_id);

    /**
     * @brief 同步所有配置了 Token 的仓库（后台任务）
     */
    static void SyncAll();
};
