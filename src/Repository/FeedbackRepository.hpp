/**
 * @file FeedbackRepository.hpp
 * @brief 用户反馈读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserFeedbackInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class FeedbackRepository {
public:
    static DB::UserFeedbackInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static bool ListByPages(std::vector<DB::UserFeedbackInfo::ptr>& infos,
        const std::string& type,
        int32_t page,
        int32_t page_size,
        int64_t& total,
        chen::IDB::ptr conn);

    static bool Insert(DB::UserFeedbackInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::UserFeedbackInfo::ptr info, chen::IDB::ptr conn);
};
