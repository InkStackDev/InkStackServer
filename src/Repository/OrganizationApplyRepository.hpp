/**
 * @file OrganizationApplyRepository.hpp
 * @brief 组织申请读写与缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/OrganizationApplyInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class OrganizationApplyRepository {
public:
    static DB::OrganizationApplyInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool Insert(DB::OrganizationApplyInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::OrganizationApplyInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::OrganizationApplyInfo::ptr& info);

    static void Evict(int64_t id);

    static int64_t ListByPages(std::vector<DB::OrganizationApplyInfo::ptr>& results,
        uint64_t offset,
        uint64_t limit,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    static int64_t ListByUser(std::vector<DB::OrganizationApplyInfo::ptr>& results,
        int64_t user_id,
        uint64_t offset,
        uint64_t limit,
        bool is_valid,
        chen::IDB::ptr conn);
};
