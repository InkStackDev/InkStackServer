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

    /**
     * @brief 批量统计每个用户的文章数（未删除）
     */
    static void CountByUsers(const std::vector<int64_t>& user_ids,
        std::vector<std::pair<int64_t, int64_t>>& rows,
        chen::IDB::ptr conn);

    /**
     * @brief 已发布且未删除文章的浏览总数
     */
    static int64_t SumPublishedViews(chen::IDB::ptr conn);

    /**
     * @brief 某用户在某时间段内已发布文章的日期（不重复）
     */
    static void ListPublishedDays(int64_t user_id,
        const std::string& start_time,
        const std::string& end_time,
        std::vector<int32_t>& days,
        chen::IDB::ptr conn);

    /**
     * @brief 搜索快照分批读取（id > last_id，按 id 升序）
     */
    static bool ListSnapshotAfter(int64_t last_id,
        int32_t limit,
        std::vector<DB::ArticleInfo::ptr>& infos,
        chen::IDB::ptr conn);
};
