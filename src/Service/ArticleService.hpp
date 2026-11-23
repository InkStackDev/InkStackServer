/**
 * @file ArticleService.hpp
 * @brief 文章用例：删除等需要事务与缓存一致性的操作
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ArticleInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class ArticleService {
public:
    struct CreateArticleInput {
        int64_t user_id = 0;
        std::string title;
        std::string content;
        int32_t type = 0;
        std::vector<int64_t> category_ids;
        std::vector<int64_t> label_ids;
    };

    struct UpdateArticleInput {
        int64_t article_id = 0;
        int64_t user_id = 0;
        std::string title;
        std::string content;
        bool has_type = false;
        int32_t type = 0;
        bool has_channel = false;
        int64_t channel = 0;
        bool has_category = false;
        bool has_label = false;
        std::vector<int64_t> category_ids;
        std::vector<int64_t> label_ids;
    };

    enum class CreateArticleResult {
        Ok,
        InvalidType,
        DbError
    };

    enum class UpdateArticleResult {
        Ok,
        NotFound,
        AccessDenied,
        InvalidType,
        DbError
    };

    enum class DeleteArticlesResult {
        Ok,
        DbError
    };

    enum class SwitchStateResult {
        Ok,
        ArticleNotFound,
        UserNotFound,
        AccessDenied,
        InvalidState,
        DbError
    };

    enum class VerifyArticleResult {
        Ok,
        ArticleNotFound,
        UserNotFound,
        AccessDenied,
        InvalidState,
        DbError
    };

    enum class PublishArticleResult {
        Ok,
        ArticleNotFound,
        InvalidState,
        DbError
    };

    /**
     * @brief 创建文章并同步分类/标签关系
     */
    static CreateArticleResult CreateArticle(const CreateArticleInput& input, DB::ArticleInfo::ptr& out);

    /**
     * @brief 更新文章并按需同步分类/标签关系
     */
    static UpdateArticleResult UpdateArticle(const UpdateArticleInput& input, DB::ArticleInfo::ptr& out);

    /**
     * @brief 切换文章状态
     * @details 管理员可切换任意状态；作者只能切到 PRIVATE。
     */
    static SwitchStateResult SwitchState(int64_t uid, int64_t article_id, int32_t new_state);

    /**
     * @brief 审核文章（管理员 / 审核员）
     */
    static VerifyArticleResult VerifyArticle(int64_t uid, int64_t article_id, int32_t state);

    /**
     * @brief 作者提交发布（可定时）
     */
    static PublishArticleResult PublishArticle(int64_t uid, int64_t article_id, int64_t publish_time);

    /**
     * @brief 软删除当前用户的文章
     * @param[out] deleted 实际删除的文章 ID
     */
    static DeleteArticlesResult DeleteArticles(int64_t uid,
        const std::vector<int64_t>& ids,
        std::vector<int64_t>& deleted);
};
