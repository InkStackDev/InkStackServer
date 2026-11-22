/**
 * @file RdListService.hpp
 * @brief 研发列表用例（Scope 解析 + QueryRepository）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"
#include "../Repository/RdListQueryRepository.hpp"

#include <cstdint>
#include <vector>

class RdListService {
public:
    static bool ListTasks(const Viewer& viewer,
        int64_t org_id,
        const RdListQueryRepository::Filter& filter,
        std::vector<DB::TaskInfo::ptr>& out,
        int64_t& total);

    static bool ListRequirements(const Viewer& viewer,
        int64_t org_id,
        const RdListQueryRepository::Filter& filter,
        std::vector<DB::RequirementInfo::ptr>& out,
        int64_t& total);

    static bool ListBugs(const Viewer& viewer,
        int64_t org_id,
        const RdListQueryRepository::Filter& filter,
        std::vector<DB::BugInfo::ptr>& out,
        int64_t& total);
};
