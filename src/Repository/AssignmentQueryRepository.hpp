/**
 * @file AssignmentQueryRepository.hpp
 * @brief 作业分页与统计查询
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/AssignmentInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <utility>
#include <vector>

class AssignmentQueryRepository {
public:
    struct Stats {
        int64_t total = 0;
        int64_t active = 0;
        int64_t closed = 0;
        int64_t draft = 0;
    };

    static uint64_t ListByPages(std::vector<DB::AssignmentInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    static Stats GetStats(chen::IDB::ptr conn);
};
