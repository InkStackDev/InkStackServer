/**
 * @file OrganizationRepoPrRepository.hpp
 * @brief 组织仓库 PR 单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationRepoPrsInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class OrganizationRepoPrRepository {
public:
    static DB::OrganizationRepoPrsInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::OrganizationRepoPrsInfo::ptr GetByRepoAndPrId(int64_t repo_id,
        int32_t github_pr_id,
        chen::IDB::ptr conn);

    static int64_t ListByRepoPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t repo_id,
        const std::string& state,
        uint64_t offset,
        uint64_t limit,
        chen::IDB::ptr conn);

    static int64_t CountByRepo(int64_t repo_id, chen::IDB::ptr conn);

    static int64_t ListByOrgPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t org_id,
        const std::string& state,
        uint64_t offset,
        uint64_t limit,
        chen::IDB::ptr conn);

    static int64_t ListByUserPages(std::vector<DB::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t uid,
        const std::string& state,
        uint64_t offset,
        uint64_t limit,
        chen::IDB::ptr conn);

    static bool Insert(DB::OrganizationRepoPrsInfo::ptr info, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::OrganizationRepoPrsInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::OrganizationRepoPrsInfo::ptr info, chen::IDB::ptr conn);

    static bool DeleteById(int64_t id, chen::IDB::ptr conn);

    static void Put(const DB::OrganizationRepoPrsInfo::ptr& info);

    static void Evict(int64_t id);
};
