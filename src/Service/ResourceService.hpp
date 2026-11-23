/**
 * @file ResourceService.hpp
 * @brief 资源查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/ResourceInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class ResourceService {
public:
    static bool GetByPath(const std::string& path, DB::ResourceInfo::ptr& out);

    static bool ListByBizUid(const std::string& biz_type,
        int64_t biz_id,
        int64_t uid,
        std::vector<DB::ResourceInfo::ptr>& out);
};
