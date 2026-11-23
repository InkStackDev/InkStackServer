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
};
