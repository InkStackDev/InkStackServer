/**
 * @file FaqService.hpp
 * @brief 常见问题用例：创建、修改、删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/HelpFaqsInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class FaqService {
public:
    static bool ListAll(std::vector<DB::HelpFaqsInfo::ptr>& infos);

    static bool Search(const std::string& keyword, std::vector<DB::HelpFaqsInfo::ptr>& infos);

    static bool Create(const std::string& cat,
        const std::string& question,
        const std::string& answer,
        int32_t sort_order,
        DB::HelpFaqsInfo::ptr& out);

    static bool Update(int64_t id, const std::string& question, const std::string& answer, const std::string& cat);

    static bool Remove(int64_t id);
};
