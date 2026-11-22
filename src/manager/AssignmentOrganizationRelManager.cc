#include "AssignmentOrganizationRelManager.hpp"

#include "../Repository/AssignmentRelRepository.hpp"
#include "../Util/Util.hpp"

DB::AssignmentOrganizationRelInfo::ptr AssignmentOrganizationRelManager::get(int64_t id)
{
    return AssignmentOrganizationRelRepository::Get(id, GetDB());
}

DB::AssignmentOrganizationRelInfo::ptr AssignmentOrganizationRelManager::getByOrgAndAssign(int64_t org_id,
    int64_t assign_id)
{
    return AssignmentOrganizationRelRepository::GetByOrgAndAssign(org_id, assign_id, GetDB());
}

int64_t AssignmentOrganizationRelManager::getByAssignmentId(
    std::vector<DB::AssignmentOrganizationRelInfo::ptr>& results,
    int64_t assign_id)
{
    return AssignmentOrganizationRelRepository::ListByAssignment(results, assign_id, GetDB());
}

int64_t AssignmentOrganizationRelManager::getByPages(std::vector<DB::AssignmentOrganizationRelInfo::ptr>& results,
    int64_t o_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return AssignmentOrganizationRelRepository::ListByOrgPages(results, o_id, offset, size, status, is_valid, GetDB());
}
