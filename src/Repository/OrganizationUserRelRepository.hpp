/**
 * @file OrganizationUserRelRepository.hpp
 * @brief 组织成员关系读写与缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationUserRelInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class OrganizationUserRelRepository {
public:
    struct Stats {
        int64_t total_members = 0;
        int64_t active_members = 0;
        int64_t org_admin_count = 0;
        int64_t regular_count = 0;
    };

    static DB::OrganizationUserRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::OrganizationUserRelInfo::ptr GetByOrgAndUser(int64_t org_id, int64_t user_id, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::OrganizationUserRelInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::OrganizationUserRelInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::OrganizationUserRelInfo::ptr& info);

    static void Evict(int64_t id);

    static int64_t ListByOrgPages(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    static int64_t ListByUser(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
        int64_t user_id,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    static int64_t ListByOrg(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
        int64_t org_id,
        bool is_valid,
        chen::IDB::ptr conn);

    static int64_t CountByOrg(int64_t org_id, int32_t status, bool is_valid, chen::IDB::ptr conn);

    static Stats GetStats(int64_t org_id, chen::IDB::ptr conn);
};
