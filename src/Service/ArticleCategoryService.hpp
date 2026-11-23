/**
 * @file ArticleCategoryService.hpp
 * @brief 文章分类关联用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <vector>

class ArticleCategoryService {
public:
    /**
     * @brief 增删文章分类关联（只接受属于 uid 的分类）
     * @param[out] added   本次新增/复活的分类 ID
     * @param[out] removed 本次移除的分类 ID
     */
    static bool UpdateForArticle(int64_t article_id,
        int64_t uid,
        const std::vector<int64_t>& add_ids,
        const std::vector<int64_t>& del_ids,
        std::vector<int64_t>& added,
        std::vector<int64_t>& removed);
};
