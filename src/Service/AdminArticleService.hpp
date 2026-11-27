/**
 * @file AdminArticleService.hpp
 * @brief 管理端文章查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class AdminArticleService {
public:
    struct Item {
        int64_t id = 0;
        int64_t user_id = 0;
        std::string author;
        std::string author_avatar;
        std::string email;
        int32_t author_role = 0;
        bool has_author = false;
        std::string title;
        int32_t state = 0;
        int64_t views = 0;
        int64_t praise = 0;
        int64_t favorites = 0;
        int64_t publish_time = 0;
        std::string summary;
        int64_t comment_count = 0;
    };

    struct Stats {
        int64_t total = 0;
        int64_t pending = 0;
        int64_t published = 0;
        int64_t rejected = 0;
        int64_t reported = 0;
    };

    static bool List(uint64_t offset,
        int32_t state,
        int64_t category,
        int32_t role,
        int32_t days,
        uint64_t page_size,
        std::vector<Item>& out,
        int64_t& total);

    static bool GetStats(int32_t category, int32_t role, int32_t days, const std::string& keyword, Stats& out);
};
