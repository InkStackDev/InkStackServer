/**
 * @file AdminCommentService.hpp
 * @brief 管理端评论查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CommentInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class AdminCommentService {
public:
    struct Author {
        int64_t id = 0;
        std::string name;
        std::string email;
        std::string avatar;
        bool valid = false;
    };

    struct Item {
        int64_t id = 0;
        std::string content;
        int64_t parent_id = 0;
        int32_t status = 0;
        int64_t create_time = 0;
        int64_t update_time = 0;
        int64_t likes = 0;
        int64_t reply_count = 0;
        bool is_reported = false;
        int32_t report_count = 0;
        std::string ip;
        std::string user_agent;
        Author author;
        int64_t article_id = 0;
        std::string article_title;
    };

    struct Stats {
        int64_t total = 0;
        int64_t pending = 0;
        int64_t approved = 0;
        int64_t spam = 0;
        int64_t reported = 0;
    };

    static bool List(int64_t page_num,
        int64_t page_size,
        int32_t status,
        const std::string& keyword,
        int64_t article_id,
        int32_t is_reported,
        std::vector<Item>& out,
        int64_t& total);

    static bool GetStats(Stats& out);

    static bool GetComment(int64_t comment_id, DB::CommentInfo::ptr& out);
};
