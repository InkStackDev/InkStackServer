/**
 * @file OrganizationRepoService.hpp
 * @brief 组织仓库用例：增删、Token、GitHub 元数据同步
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include "DB/OrganizationReposInfo.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct OrganizationRepoInput {
    int64_t org_id = 0;
    std::string name;
    std::string url;
    std::string language;
    std::string description;
    std::string token;
};

struct OrganizationRepoUpdateInput {
    int64_t id = 0;
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> url;
    std::optional<std::string> language;
    std::optional<int32_t> sort_order;
};

class OrganizationRepoService {
public:
    enum class Result {
        Ok,
        NotFound,
        OrgNotFound,
        NameExists,
        NotMember,
        AccessDenied,
        InvalidParam,
        DbError
    };

    static Result Add(const Viewer& viewer, const OrganizationRepoInput& input, DB::OrganizationReposInfo::ptr& out);

    static Result Update(const Viewer& viewer,
        const OrganizationRepoUpdateInput& input,
        DB::OrganizationReposInfo::ptr& out);

    static Result Remove(const Viewer& viewer, int64_t id, int64_t org_id = 0);

    static Result UpdateToken(const Viewer& viewer,
        int64_t id,
        const std::string& token,
        DB::OrganizationReposInfo::ptr& out);

    static Result MarkSyncing(const Viewer& viewer, int64_t id, DB::OrganizationReposInfo::ptr& out);

    static Result List(const Viewer& viewer,
        int64_t org_id,
        uint64_t offset,
        uint64_t limit,
        std::vector<DB::OrganizationReposInfo::ptr>& out,
        int64_t& total);

    static Result CountByOrg(int64_t org_id, int64_t& total);

    /**
     * @brief 校验组织访问权限
     * @param manage true 要求 DEV_LEAD 及以上，false 只要求成员
     */
    static Result CheckOrgAccess(const Viewer& viewer, int64_t org_id, bool manage);

    /**
     * @brief 校验仓库访问权限，并返回仓库
     */
    static Result CheckRepoAccess(const Viewer& viewer,
        int64_t repo_id,
        bool manage,
        DB::OrganizationReposInfo::ptr& out);

    /**
     * @brief 从 GitHub API 同步单个仓库的 stars/language/description
     */
    static void SyncFromGitHub(int64_t repo_id);

    /**
     * @brief 同步所有配置了 Token 的仓库（后台任务）
     */
    static void SyncAll();
};
