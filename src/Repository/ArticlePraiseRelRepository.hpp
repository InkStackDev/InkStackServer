/**
 * @file ArticlePraiseRelRepository.hpp
 * @brief 文章点赞关联读写与缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ArticlePraiseRelInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class ArticlePraiseRelRepository {
public:
    static DB::ArticlePraiseRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::ArticlePraiseRelInfo::ptr GetByUserAndArticle(int64_t user_id, int64_t article_id, chen::IDB::ptr conn);

    static bool Insert(DB::ArticlePraiseRelInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::ArticlePraiseRelInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::ArticlePraiseRelInfo::ptr& info);

    static void Evict(int64_t id);

    static void ListByArticle(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
        int64_t article_id,
        uint64_t offset,
        uint64_t size,
        chen::IDB::ptr conn);

    static void ListByUser(std::vector<DB::ArticlePraiseRelInfo::ptr>& results,
        int64_t user_id,
        uint64_t offset,
        uint64_t size,
        chen::IDB::ptr conn);

    static int64_t CountByArticle(int64_t article_id, chen::IDB::ptr conn);

    static int64_t CountByUser(int64_t user_id, chen::IDB::ptr conn);
};
