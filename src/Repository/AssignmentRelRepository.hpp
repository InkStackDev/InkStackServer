/**
 * @file AssignmentRelRepository.hpp
 * @brief 作业与组织/用户关联表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/AssignmentOrganizationRelInfo.hpp"
#include "DB/AssignmentUserRelInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <vector>

class AssignmentOrganizationRelRepository {
public:
    static DB::AssignmentOrganizationRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::AssignmentOrganizationRelInfo::ptr GetByOrgAndAssign(int64_t org_id,
        int64_t assignment_id,
        chen::IDB::ptr conn);

    static bool InsertOrUpdate(DB::AssignmentOrganizationRelInfo::ptr info, chen::IDB::ptr conn);

    static int64_t ListByAssignment(std::vector<DB::AssignmentOrganizationRelInfo::ptr>& results,
        int64_t assignment_id,
        chen::IDB::ptr conn);

    static int64_t ListByOrgPages(std::vector<DB::AssignmentOrganizationRelInfo::ptr>& results,
        int64_t org_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid,
        chen::IDB::ptr conn);
};

class AssignmentUserRelRepository {
public:
    static DB::AssignmentUserRelInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::AssignmentUserRelInfo::ptr GetByAssignAndUser(int64_t assignment_id,
        int64_t user_id,
        chen::IDB::ptr conn);

    static int64_t ListByAssignment(std::vector<DB::AssignmentUserRelInfo::ptr>& results,
        int64_t assignment_id,
        bool is_valid,
        chen::IDB::ptr conn);
};
