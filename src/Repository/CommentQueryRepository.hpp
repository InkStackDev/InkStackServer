/**
 * @file CommentQueryRepository.hpp
 * @brief 评论列表/计数/统计查询
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CommentInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class CommentQueryRepository {
public:
    struct Stats {
        int64_t total = 0;
        int64_t pending = 0;
        int64_t approved = 0;
        int64_t spam = 0;
        int64_t reported = 0;
    };

    static void ListAllByArticle(std::vector<DB::CommentInfo::ptr>& results, int64_t article_id, chen::IDB::ptr conn);

    static void ListByArticle(std::vector<DB::CommentInfo::ptr>& results,
        int64_t article_id,
        uint64_t offset,
        uint64_t size,
        chen::IDB::ptr conn);

    static void ListReplies(std::vector<DB::CommentInfo::ptr>& results,
        int64_t parent_id,
        uint64_t offset,
        uint64_t size,
        chen::IDB::ptr conn);

    static int64_t CountByArticle(int64_t article_id, chen::IDB::ptr conn);

    static int64_t CountReplies(int64_t parent_id, chen::IDB::ptr conn);

    static int64_t ListByAdmin(std::vector<DB::CommentInfo::ptr>& results,
        int64_t page_num,
        int64_t page_size,
        int32_t status,
        const std::string& keyword,
        int64_t article_id,
        int32_t is_reported,
        chen::IDB::ptr conn);

    static Stats GetStats(chen::IDB::ptr conn);

    /**
     * @brief 某篇文章的评论发生变化，失效其计数/列表缓存
     */
    static void InvalidateArticle(int64_t article_id);

    /**
     * @brief 某条评论的回复发生变化，失效其回复数缓存
     */
    static void InvalidateReplies(int64_t parent_id);

    /**
     * @brief 评论状态/可见性变化，失效统计缓存
     */
    static void InvalidateStats();
};
