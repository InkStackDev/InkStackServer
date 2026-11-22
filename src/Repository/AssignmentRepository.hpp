/**
 * @file AssignmentRepository.hpp
 * @brief 作业单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/AssignmentInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>

class AssignmentRepository {
public:
    static DB::AssignmentInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::AssignmentInfo::ptr GetByName(const std::string& subject_name,
        const std::string& name,
        chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::AssignmentInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::AssignmentInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::AssignmentInfo::ptr& info);

    static void Evict(int64_t id);

    /**
     * @brief 名称变更后失效 name → id 映射缓存
     */
    static void EvictName(const std::string& subject_name, const std::string& name);
};
