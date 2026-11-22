/**
 * @file CategoryQueryService.hpp
 * @brief 分类查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class CategoryQueryService {
public:
    struct Item {
        int64_t id = 0;
        int64_t user_id = 0;
        std::string author;
        std::string name;
        std::string url;
        std::string icon;
        std::string color;
        std::string description;
        int64_t parent_id = 0;
        int32_t status = 0;
        int64_t create_time = 0;
        int64_t update_time = 0;
        int64_t article_count = 0;
        int64_t view_count = 0;
    };

    static bool List(int64_t requester_uid, bool admin_route, int64_t owner_uid, std::vector<Item>& out);
};
