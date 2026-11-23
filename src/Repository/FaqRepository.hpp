/**
 * @file FaqRepository.hpp
 * @brief 常见问题读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/HelpFaqsInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class FaqRepository {
public:
    static DB::HelpFaqsInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static void ListValid(std::vector<DB::HelpFaqsInfo::ptr>& infos, chen::IDB::ptr conn);

    static bool Insert(DB::HelpFaqsInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::HelpFaqsInfo::ptr info, chen::IDB::ptr conn);
};
