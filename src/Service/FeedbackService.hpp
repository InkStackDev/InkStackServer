/**
 * @file FeedbackService.hpp
 * @brief 用户反馈用例：提交与删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserFeedbackInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class FeedbackService {
public:
    static bool List(std::vector<DB::UserFeedbackInfo::ptr>& infos,
        const std::string& type,
        int32_t page,
        int32_t page_size,
        int64_t& total);

    static DB::UserFeedbackInfo::ptr Get(int64_t id);

    static bool Submit(int64_t uid,
        const std::string& type,
        const std::string& content,
        const std::string& contact,
        DB::UserFeedbackInfo::ptr& out);

    static bool Remove(int64_t id);
};
