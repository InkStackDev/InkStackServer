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
    struct ListFilter {
        int64_t user_id = 0;
        int32_t status = -1;
        std::string keyword;
        std::string sort_by = "name";
        std::string sort_order = "asc";
    };

    struct ListItem {
        int64_t id = 0;
        int64_t user_id = 0;
        int64_t parent_id = 0;
        int32_t status = 0;
        int64_t create_time = 0;
        int64_t update_time = 0;
        int64_t article_count = 0;
        int64_t view_count = 0;
        std::string name;
        std::string url;
        std::string icon;
        std::string color;
        std::string description;
    };

    struct Stats {
        int64_t total = 0;
        int64_t active = 0;
        int64_t articles = 0;
        int64_t views = 0;
    };

    static DB::CategoryInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::CategoryInfo::ptr GetByUserIdName(int64_t user_id, const std::string& name, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::CategoryInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::CategoryInfo::ptr info, chen::IDB::ptr conn);

    static void ListAll(std::vector<DB::CategoryInfo::ptr>& infos, bool is_valid, chen::IDB::ptr conn);

    static bool ListByUserId(std::vector<DB::CategoryInfo::ptr>& infos,
        int64_t user_id,
        bool is_valid,
        chen::IDB::ptr conn);

    static int64_t ListByPages(std::vector<ListItem>& out,
        uint64_t offset,
        uint64_t limit,
        const ListFilter& filter,
        bool is_valid,
        chen::IDB::ptr conn);

    static Stats GetStats(chen::IDB::ptr conn);

    static void Put(const DB::CategoryInfo::ptr& info);

    static void Evict(int64_t id);

    static void EvictUserIdName(int64_t user_id, const std::string& name);
};
