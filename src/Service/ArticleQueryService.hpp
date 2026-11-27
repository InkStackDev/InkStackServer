/**
 * @file ArticleQueryService.hpp
 * @brief 文章查询与详情用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"
#include "DB/ArticleInfo.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class ArticleQueryService {
public:
    struct CategoryBrief {
        int64_t id = 0;
        std::string name;
        std::string color;
    };

    struct ListItem {
        int64_t id = 0;
        int64_t user_id = 0;
        std::string author;
        std::string author_avatar;
        std::string title;
        std::string summary;
        int32_t type = 0;
        int32_t state = 0;
        int64_t views = 0;
        int64_t praise = 0;
        int64_t favorites = 0;
        int64_t publish_time = 0;
        int64_t comment_count = 0;
        std::vector<CategoryBrief> categories;
    };

    struct Detail {
        int64_t id = 0;
        int64_t user_id = 0;
        std::string author;
        std::string author_avatar;
        int64_t author_article_count = 0;
        int64_t author_praise_count = 0;
        int64_t author_follower_count = 0;
        std::string title;
        std::string content;
        int32_t type = 0;
        int64_t publish_time = 0;
        int64_t update_time = 0;
        int32_t state = 0;
        int32_t is_deleted = 0;
        int64_t views = 0;
        int64_t praise = 0;
        int64_t favorites = 0;
        std::vector<CategoryBrief> categories;
        std::vector<CategoryBrief> labels;
    };

    struct PraiseUser {
        int64_t id = 0;
        int64_t user_id = 0;
        int64_t article_id = 0;
        int64_t create_time = 0;
        std::string user_name;
        std::string user_avatar;
    };

    struct PraiseArticle {
        int64_t id = 0;
        int64_t user_id = 0;
        int64_t article_id = 0;
        int64_t create_time = 0;
        std::string title;
    };

    enum class DetailResult {
        Ok,
        NotFound,
        AccessDenied,
        DbError
    };

    static bool ListByUser(int64_t user_id,
        int64_t offset,
        int64_t page_size,
        int32_t state,
        std::vector<ListItem>& out,
        int64_t& total);

    static bool ListByCategory(int64_t category_id,
        int64_t offset,
        int64_t page_size,
        std::vector<ListItem>& out,
        int64_t& total);

    static bool ListByLabel(int64_t label_id,
        int64_t offset,
        int64_t page_size,
        std::vector<ListItem>& out,
        int64_t& total);

    static DetailResult GetDetail(const Viewer& viewer, int64_t article_id, uint32_t type, Detail& out);

    static bool GetArticle(int64_t article_id, DB::ArticleInfo::ptr& out);

    static bool ListPraiseUsers(int64_t article_id,
        uint64_t offset,
        uint64_t size,
        std::vector<PraiseUser>& out,
        int64_t& total);

    static bool ListPraisedArticles(int64_t user_id,
        uint64_t offset,
        uint64_t size,
        std::vector<PraiseArticle>& out,
        int64_t& total);
};
