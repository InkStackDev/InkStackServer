/**
 * @file ArticleQueryRepository.hpp
 * @brief 文章列表/统计查询
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ArticleInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class ArticleQueryRepository {
public:
    struct Stats {
        int64_t total = 0;
        int64_t pending = 0;
        int64_t published = 0;
        int64_t rejected = 0;
        int64_t reported = 0;
    };

    static bool ListByUserId(std::vector<DB::ArticleInfo::ptr>& infos,
        int64_t user_id,
        bool valid,
        chen::IDB::ptr conn);

    static int64_t ListByUserIdPages(std::vector<DB::ArticleInfo::ptr>& infos,
        int64_t user_id,
        int32_t offset,
        int32_t size,
        bool valid,
        int32_t state,
        chen::IDB::ptr conn);

    static int64_t ListVerifyPages(std::vector<DB::ArticleInfo::ptr>& infos,
        int32_t offset,
        int32_t size,
        chen::IDB::ptr conn);

    static int64_t ListByPages(std::vector<DB::ArticleInfo::ptr>& infos,
        int32_t offset,
        int32_t state,
        int32_t category,
        int32_t role,
        int32_t days,
        int32_t size,
        bool valid,
        chen::IDB::ptr conn);

    static int64_t ListByLabelPages(std::vector<DB::ArticleInfo::ptr>& infos,
        int64_t label_id,
        int32_t offset,
        int32_t size,
        bool valid,
        chen::IDB::ptr conn);

    static int64_t ListByCategoryPages(std::vector<DB::ArticleInfo::ptr>& infos,
        int64_t category_id,
        int32_t offset,
        int32_t size,
        bool valid,
        chen::IDB::ptr conn);

    static std::pair<DB::ArticleInfo::ptr, DB::ArticleInfo::ptr> Nearby(int64_t id, chen::IDB::ptr conn);

    static Stats GetStats(int32_t category,
        int32_t role,
        int32_t days,
        const std::string& keyword,
        chen::IDB::ptr conn);
};
