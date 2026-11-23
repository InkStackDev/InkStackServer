/**
 * @file OrganizationRepoRepository.hpp
 * @brief 组织仓库单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationReposInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class OrganizationRepoRepository {
public:
    static DB::OrganizationReposInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::OrganizationReposInfo::ptr GetByOrgAndName(int64_t org_id, const std::string& name, chen::IDB::ptr conn);

    static int64_t ListByOrgPages(std::vector<DB::OrganizationReposInfo::ptr>& repos,
        int64_t org_id,
        uint64_t offset,
        uint64_t limit,
        chen::IDB::ptr conn);

    static int64_t CountByOrg(int64_t org_id, chen::IDB::ptr conn);

    static void ListWithToken(std::vector<DB::OrganizationReposInfo::ptr>& repos, chen::IDB::ptr conn);

    /**
     * @brief 按 github_full_name 精确查找仓库 ID（webhook 反查用）
     * @return 0 表示未找到
     */
    static int64_t FindIdByGithubFullName(const std::string& full_name, chen::IDB::ptr conn);

    static bool Insert(DB::OrganizationReposInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::OrganizationReposInfo::ptr info, chen::IDB::ptr conn);

    static bool DeleteById(int64_t id, chen::IDB::ptr conn);

    static void Put(const DB::OrganizationReposInfo::ptr& info);

    static void Evict(int64_t id);

    /**
     * @brief 仓库数量变化后失效组织维度的计数/列表缓存
     */
    static void InvalidateOrgCount(int64_t org_id);
};
