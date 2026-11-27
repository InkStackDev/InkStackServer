/**
 * @file ArticleSearchService.hpp
 * @brief 文章搜索用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "protocol_ss_search.h"

#include <cstdint>
#include <string>
#include <vector>

class ArticleSearchService {
public:
    struct Item {
        int64_t id = 0;
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
    };

    struct Result {
        int64_t total = 0;
        std::vector<Item> items;
    };

    /**
     * @brief 公共搜索：只查已发布文章
     * @return false 表示搜索服务不可用
     */
    static bool Search(const tagSearchQuery& query, Result& out);
};
