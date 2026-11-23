/**
 * @file LabelService.hpp
 * @brief 标签用例：创建/更新与批量删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/LabelInfo.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

class LabelService {
public:
    enum class Result {
        Ok,
        NotFound,
        AccessDenied,
        DbError
    };

    static Result CreateOrUpdate(int64_t uid,
        int64_t id,
        const std::string& name,
        const std::string& color,
        const std::string& description,
        DB::LabelInfo::ptr& out);

    static bool DeleteMany(int64_t uid, const std::set<int64_t>& ids, std::vector<DB::LabelInfo::ptr>& deleted);

    struct LabelView {
        int64_t id = 0;
        std::string name;
        std::string color;
        std::string description;
        int64_t create_time = 0;
        int64_t article_count = 0;
    };

    static bool List(int64_t user_id, const std::vector<int64_t>& ids, std::vector<LabelView>& out);
};
