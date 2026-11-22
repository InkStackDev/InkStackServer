/**
 * @file ArticleRepository.hpp
 * @brief 文章单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ArticleInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>

/**
 * @brief 文章 Repository
 * @details 只做单表 CRUD 与对象缓存；所有方法显式接收 conn。
 */
class ArticleRepository {
public:
    static DB::ArticleInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool Insert(DB::ArticleInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::ArticleInfo::ptr info, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::ArticleInfo::ptr info, chen::IDB::ptr conn);

    /**
     * @brief 写入对象缓存（DB commit 成功后调用）
     */
    static void Put(const DB::ArticleInfo::ptr& info);

    /**
     * @brief 删除对象缓存
     */
    static void Evict(int64_t id);
};
