/**
 * @file ArticleService.hpp
 * @brief 文章用例：删除等需要事务与缓存一致性的操作
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <vector>

class ArticleService {
public:
    enum class DeleteArticlesResult {
        Ok,
        DbError
    };

    /**
     * @brief 软删除当前用户的文章
     * @param[out] deleted 实际删除的文章 ID
     */
    static DeleteArticlesResult DeleteArticles(int64_t uid,
        const std::vector<int64_t>& ids,
        std::vector<int64_t>& deleted);
};
