/**
 * @file AdminAssignmentService.hpp
 * @brief 管理端作业查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class AdminAssignmentService {
public:
    struct Item {
        int64_t id = 0;
        std::string name;
        std::string subject_name;
        int64_t end_time = 0;
        int32_t status = 0;
        int32_t priority = 0;
        int64_t create_time = 0;
        std::string description;
        std::string file_type;
        int64_t file_size = 0;
        std::string organization_name;
        std::string assigned_by;
    };

    struct Stats {
        int64_t total = 0;
        int64_t active = 0;
        int64_t closed = 0;
        int64_t draft = 0;
    };

    static bool List(uint64_t offset, uint64_t page_size, int32_t state, std::vector<Item>& out, int64_t& total);

    static bool GetStats(Stats& out);
};
