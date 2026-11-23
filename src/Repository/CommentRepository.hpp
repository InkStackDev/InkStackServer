/**
 * @file CommentRepository.hpp
 * @brief 评论单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/CommentInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class CommentRepository {
public:
    static DB::CommentInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool Insert(DB::CommentInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::CommentInfo::ptr info, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::CommentInfo::ptr info, chen::IDB::ptr conn);

    static void Put(const DB::CommentInfo::ptr& info);

    static void Evict(int64_t id);

    /**
     * @brief 批量修改评论状态（绕过对象缓存，调用方负责刷新缓存）
     */
    static bool BatchSetStatus(const std::vector<int64_t>& ids,
        int32_t status,
        int64_t update_time,
        chen::IDB::ptr conn);

    /**
     * @brief 批量软删除评论
     */
    static bool BatchSoftDelete(const std::vector<int64_t>& ids, int64_t update_time, chen::IDB::ptr conn);
};
