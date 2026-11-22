/**
 * @file GitHubClient.hpp
 * @brief GitHub API 客户端（网络治理 + DTO）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "ClientError.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct GitHubRepoInfo {
    int32_t stars = 0;
    std::string language;
    std::string description;
    std::string full_name;
};

struct GitHubPullRequest {
    int32_t number = 0;
    std::string title;
    std::string description;
    std::string state;
    std::string author;
    std::string head_branch;
    std::string base_branch;
    std::string commit_sha;
    bool merged = false;
    int64_t created_at = 0;
    int64_t closed_at = 0;
    int64_t merged_at = 0;
    int32_t additions = 0;
    int32_t deletions = 0;
    int32_t changed_files = 0;
    bool has_detail = false;
    bool has_reviews = false;
    std::string review_status = "pending";
    std::string reviewers = "[]";
};

struct GitHubReviewSummary {
    std::string status = "pending";
    std::string reviewers = "[]";
};

class GitHubClient {
public:
    /**
     * @brief 获取仓库元数据（stars / language / description / full_name）
     * @param token 为空表示匿名请求
     * @param repo_path owner/repo
     */
    static ClientResult<GitHubRepoInfo> GetRepo(const std::string& token, const std::string& repo_path);

    /**
     * @brief 分页拉取 PR 列表
     * @param per_page 每页条数，<=0 使用配置
     * @param max_pages 最大页数，<=0 使用 10
     */
    static ClientResult<std::vector<GitHubPullRequest>> ListPullRequests(const std::string& token,
        const std::string& repo_path,
        int32_t per_page,
        int32_t max_pages);

    /**
     * @brief 拉取单个 PR 详情（含 additions/deletions/changed_files）
     */
    static ClientResult<GitHubPullRequest> GetPullRequest(const std::string& token,
        const std::string& repo_path,
        int32_t number);

    /**
     * @brief 拉取 PR 评审汇总
     */
    static ClientResult<GitHubReviewSummary> GetReviewSummary(const std::string& token,
        const std::string& repo_path,
        int32_t number);
};
