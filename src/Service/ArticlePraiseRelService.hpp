/**
 * @file ArticlePraiseRelService.hpp
 * @brief 文章点赞用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ArticlePraiseRelInfo.hpp"

#include <cstdint>

class ArticlePraiseRelService {
public:
    /**
     * @brief 点赞（软删除记录会复活）
     */
    static DB::ArticlePraiseRelInfo::ptr Praise(int64_t user_id, int64_t article_id);

    static bool Unpraise(int64_t user_id, int64_t article_id);

    static bool IsPraising(int64_t user_id, int64_t article_id);
};
