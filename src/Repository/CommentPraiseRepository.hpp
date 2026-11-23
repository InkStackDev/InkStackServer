/**
 * @file CommentPraiseRepository.hpp
 * @brief 评论点赞关联读写与缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CommentPraiseRelInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>

class CommentPraiseRepository {
public:
    static DB::CommentPraiseRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::CommentPraiseRelInfo::ptr GetByUserAndComment(int64_t user_id, int64_t comment_id, chen::IDB::ptr conn);

    static bool Insert(DB::CommentPraiseRelInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::CommentPraiseRelInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::CommentPraiseRelInfo::ptr& info);

    static void Evict(int64_t id);

    static int64_t CountByComment(int64_t comment_id, chen::IDB::ptr conn);

    static bool IsPraising(int64_t user_id, int64_t comment_id, chen::IDB::ptr conn);
};
