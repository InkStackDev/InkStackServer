/**
 * @file CategoryRepository.hpp
 * @brief 分类单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CategoryInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class CategoryRepository {
public:
    static DB::CategoryInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::CategoryInfo::ptr GetByUserIdName(int64_t user_id, const std::string& name, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::CategoryInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::CategoryInfo::ptr info, chen::IDB::ptr conn);

    static void ListAll(std::vector<DB::CategoryInfo::ptr>& infos, bool is_valid, chen::IDB::ptr conn);

    static bool ListByUserId(std::vector<DB::CategoryInfo::ptr>& infos,
        int64_t user_id,
        bool is_valid,
        chen::IDB::ptr conn);

    static void Put(const DB::CategoryInfo::ptr& info);

    static void Evict(int64_t id);

    static void EvictUserIdName(int64_t user_id, const std::string& name);
};
