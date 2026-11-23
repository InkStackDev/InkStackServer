/**
 * @file SystemSettingsRepository.hpp
 * @brief 系统设置单行表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/SystemSettingsInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>

class SystemSettingsRepository {
public:
    static DB::SystemSettingsInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool Insert(DB::SystemSettingsInfo::ptr info, chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::SystemSettingsInfo::ptr info, chen::IDB::ptr conn);
};
