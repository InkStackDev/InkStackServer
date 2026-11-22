/**
 * @file TaskRepository.hpp
 * @brief 任务单表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/TaskInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class TaskRepository {
public:
    static DB::TaskInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::TaskInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::TaskInfo::ptr info, chen::IDB::ptr conn);

    static uint64_t ListByPages(std::vector<DB::TaskInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    static uint64_t ListByOrg(std::vector<DB::TaskInfo::ptr>& infos,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);

    /**
     * @brief 统计指定组织集合在 before 之前创建的任务数（趋势图用）
     */
    static int64_t CountCreatedBefore(const std::vector<int64_t>& org_ids, int64_t before, chen::IDB::ptr conn);
};
