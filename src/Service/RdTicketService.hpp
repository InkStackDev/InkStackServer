/**
 * @file RdTicketService.hpp
 * @brief “我的研发单据”查询用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

class RdTicketService {
public:
    enum class Result {
        Ok,
        InvalidType,
        DbError
    };

    struct Item {
        int64_t id = 0;
        std::string title;
        std::string description;
        int32_t status = 0;
        int32_t priority = 0;
        int32_t severity = 0;
        int64_t creator_id = 0;
        std::string creator;
        int64_t assignee_id = 0;
        std::string assignee;
        std::string assignee_avatar;
        int64_t org_id = 0;
        std::string org_name;
        std::string module;
        int64_t deadline = 0;
        int64_t created_at = 0;
        int64_t updated_at = 0;
    };

    static Result ListMine(const Viewer& viewer,
        const std::string& type,
        int64_t org_id,
        int32_t status,
        const std::string& search,
        uint64_t offset,
        uint64_t limit,
        std::vector<Item>& out,
        int64_t& total);
};
