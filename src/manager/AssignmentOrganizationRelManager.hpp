#pragma once

#include "blog/data/assignment_organization_rel_info.h"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class AssignmentOrganizationRelManager {
public:
    blog::data::AssignmentOrganizationRelInfo::ptr get(int64_t id);

    blog::data::AssignmentOrganizationRelInfo::ptr getByOrgAndAssign(int64_t org_id, int64_t assign_id);

    int64_t getByAssignmentId(std::vector<data::AssignmentOrganizationRelInfo::ptr>& results, int64_t assign_id);

    int64_t getByPages(
        std::vector<data::AssignmentOrganizationRelInfo::ptr>& results,
        int64_t o_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool is_valid);

private:
    static data::AssignmentOrganizationRelInfo::ptr ParseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<AssignmentOrganizationRelManager> AssignmentOrganizationRelMgr;
