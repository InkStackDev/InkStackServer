/**
 * @file ResourceRepository.hpp
 * @brief 资源元数据读写与缓存
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ResourceInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class ResourceRepository {
public:
    static DB::ResourceInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static void ListByBizUid(std::vector<DB::ResourceInfo::ptr>& results,
        const std::string& biz_type,
        int64_t biz_id,
        int64_t uid,
        chen::IDB::ptr conn);

    static DB::ResourceInfo::ptr GetByBizUidName(const std::string& biz_type,
        int64_t biz_id,
        int64_t uid,
        const std::string& filename,
        chen::IDB::ptr conn);

    static DB::ResourceInfo::ptr GetByPath(const std::string& path, chen::IDB::ptr conn);

    static void Put(const DB::ResourceInfo::ptr& info);

    static void Evict(int64_t id);
};
