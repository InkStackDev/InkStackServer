/**
 * @file UserRepository.hpp
 * @brief 用户单表读写与对象缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief 用户 Repository
 * @details 只做单表 CRUD 与缓存；所有方法显式接收 conn，不自行获取连接，
 *          便于 Service 统一控制事务。
 */
class UserRepository {
public:
    static DB::UserInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::UserInfo::ptr GetByAccount(const std::string& account, chen::IDB::ptr conn);

    static DB::UserInfo::ptr GetByEmail(const std::string& email, chen::IDB::ptr conn);

    static DB::UserInfo::ptr GetByName(const std::string& name, chen::IDB::ptr conn);

    static bool Insert(DB::UserInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::UserInfo::ptr info, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::UserInfo::ptr info, chen::IDB::ptr conn);

    static void GetAllIds(std::vector<int64_t>& ids, bool is_valid, chen::IDB::ptr conn);

    static uint64_t ListByPages(std::vector<DB::UserInfo::ptr>& infos,
        uint64_t offset,
        uint64_t size,
        int32_t role,
        int32_t state,
        int32_t days,
        bool is_valid,
        chen::IDB::ptr conn);

    /**
     * @brief 写入对象缓存（DB commit 成功后调用）
     */
    static void Put(const DB::UserInfo::ptr& info);

    /**
     * @brief 删除对象缓存
     */
    static void Evict(int64_t id);
};
