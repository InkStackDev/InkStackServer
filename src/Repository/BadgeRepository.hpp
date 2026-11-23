/**
 * @file BadgeRepository.hpp
 * @brief 徽章定义读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/BadgeInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class BadgeRepository {
public:
    static int64_t CountAll(chen::IDB::ptr conn);

    static void ListValid(std::vector<DB::BadgeInfo::ptr>& badges, chen::IDB::ptr conn);

    static bool Insert(DB::BadgeInfo::ptr info, chen::IDB::ptr conn);
};
