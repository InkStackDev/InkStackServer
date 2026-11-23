/**
 * @file ArticleRelRepository.hpp
 * @brief 文章-分类/标签关联表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ArticleCategoryRelInfo.hpp"
#include "DB/ArticleLabelRelInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <utility>
#include <vector>

class ArticleCategoryRelRepository {
public:
    static DB::ArticleCategoryRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::ArticleCategoryRelInfo::ptr GetByArticleAndCategory(int64_t article_id,
        int64_t category_id,
        chen::IDB::ptr conn);

    static bool Insert(DB::ArticleCategoryRelInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::ArticleCategoryRelInfo::ptr info, chen::IDB::ptr conn);

    static bool ListByArticle(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
        int64_t article_id,
        bool is_valid,
        chen::IDB::ptr conn);

    static bool ListByCategory(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
        int64_t category_id,
        bool is_valid,
        chen::IDB::ptr conn);

    /**
     * @brief 批量读取多篇文章的有效分类关联（搜索索引构建用）
     */
    static bool ListByArticles(std::vector<DB::ArticleCategoryRelInfo::ptr>& infos,
        const std::vector<int64_t>& article_ids,
        chen::IDB::ptr conn);
};

class ArticleLabelRelRepository {
public:
    static DB::ArticleLabelRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::ArticleLabelRelInfo::ptr GetByArticleAndLabel(int64_t article_id, int64_t label_id, chen::IDB::ptr conn);

    static bool Insert(DB::ArticleLabelRelInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::ArticleLabelRelInfo::ptr info, chen::IDB::ptr conn);

    static bool ListByArticle(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
        int64_t article_id,
        bool is_valid,
        chen::IDB::ptr conn);

    static bool ListByLabel(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
        int64_t label_id,
        bool is_valid,
        chen::IDB::ptr conn);

    /**
     * @brief 批量读取多篇文章的有效标签关联（搜索索引构建用）
     */
    static bool ListByArticles(std::vector<DB::ArticleLabelRelInfo::ptr>& infos,
        const std::vector<int64_t>& article_ids,
        chen::IDB::ptr conn);

    /**
     * @brief 统计每个标签下的已发布文章数（标签列表页用）
     */
    static void CountPublishedByLabels(const std::vector<int64_t>& label_ids,
        std::vector<std::pair<int64_t, int64_t>>& rows,
        chen::IDB::ptr conn);
};
