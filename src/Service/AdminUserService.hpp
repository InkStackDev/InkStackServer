/**
 * @file AdminUserService.hpp
 * @brief 管理端用户查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class AdminUserService {
public:
    struct ListItem {
        int64_t id = 0;
        std::string name;
        std::string email;
        std::string avatar;
        int32_t role = 0;
        int32_t state = 0;
        int64_t create_time = 0;
        int64_t login_time = 0;
        int64_t article_count = 0;
        int64_t comment_count = 0;
    };

    struct Stats {
        int64_t total = 0;
        int64_t active = 0;
        int64_t inactive = 0;
        int64_t new_30d = 0;
    };

    struct Detail {
        int64_t id = 0;
        std::string account;
        std::string name;
        std::string email;
        std::string avatar;
        int32_t role = 0;
        int32_t state = 0;
        int64_t create_time = 0;
        int64_t login_time = 0;
        std::string bio;
        std::string website;
        int64_t article_count = 0;
        int64_t comment_count = 0;
    };

    static bool List(uint64_t offset,
        uint64_t page_size,
        int32_t role,
        int32_t state,
        int32_t days,
        std::vector<ListItem>& out,
        uint64_t& total);

    static bool GetStats(Stats& out);

    static bool GetDetail(int64_t user_id, Detail& out);
};
