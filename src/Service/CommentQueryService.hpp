/**
 * @file CommentQueryService.hpp
 * @brief 评论列表查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CommentInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class CommentQueryService {
public:
    struct Item {
        int64_t id = 0;
        std::string content;
        int64_t user_id = 0;
        std::string user;
        std::string avatar;
        int64_t create_time = 0;
        int64_t likes = 0;
        bool is_liked = false;
        int64_t reply_count = 0;
        std::vector<Item> replies;
    };

    enum class ListResult {
        Ok,
        ArticleNotFound,
        CommentNotFound,
        DbError
    };

    static ListResult ListTree(int64_t article_id, int64_t viewer_uid, std::vector<Item>& out, int64_t& total);

    static ListResult ListReplies(int64_t comment_id,
        int64_t viewer_uid,
        int64_t offset,
        int64_t size,
        std::vector<Item>& out,
        int64_t& total);

    static bool GetParent(int64_t comment_id, DB::CommentInfo::ptr& out);
};
