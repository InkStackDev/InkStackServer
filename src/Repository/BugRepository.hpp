/**
 * @file BugRepository.hpp
 * @brief 缺陷单表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/BugInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class BugRepository {
public:
    static DB::BugInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::BugInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::BugInfo::ptr info, chen::IDB::ptr conn);

    static uint64_t ListByPages(std::vector<DB::BugInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    static uint64_t ListByOrg(std::vector<DB::BugInfo::ptr>& infos,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);
};
