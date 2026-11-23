/**
 * @file RequirementRepository.hpp
 * @brief 需求单表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/RequirementInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class RequirementRepository {
public:
    static DB::RequirementInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool Insert(DB::RequirementInfo::ptr info, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::RequirementInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::RequirementInfo::ptr info, chen::IDB::ptr conn);

    static uint64_t ListByPages(std::vector<DB::RequirementInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    static uint64_t ListByOrg(std::vector<DB::RequirementInfo::ptr>& infos,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);
};
