/**
 * @file OrganizationRepository.hpp
 * @brief 组织单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class OrganizationRepository {
public:
    static DB::OrganizationInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::OrganizationInfo::ptr GetByName(const std::string& name, chen::IDB::ptr conn);

    static bool Insert(DB::OrganizationInfo::ptr info, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::OrganizationInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::OrganizationInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::OrganizationInfo::ptr& info);

    static void Evict(int64_t id);

    static void EvictName(const std::string& name);
};

class OrganizationQueryRepository {
public:
    struct Stats {
        int64_t total = 0;
        int64_t active = 0;
        int64_t inactive = 0;
    };

    static int64_t ListByPages(std::vector<DB::OrganizationInfo::ptr>& orgs,
        uint64_t offset,
        uint64_t limit,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    /**
     * @brief 查询用户已批准的组织授权（排除已删除/停用组织）
     * @param[out] grants (org_id, role) 列表
     */
    static bool ListUserOrgGrants(std::vector<std::pair<int64_t, int32_t>>& grants,
        int64_t user_id,
        chen::IDB::ptr conn);

    static Stats GetStats(chen::IDB::ptr conn);

    /**
     * @brief 组织创建/状态变更后失效统计缓存
     */
    static void InvalidateStats();
};
