#include "OrganizationApplyManager.hpp"

#include "../Repository/OrganizationApplyRepository.hpp"
#include "../Util/Util.hpp"

DB::OrganizationApplyInfo::ptr OrganizationApplyManager::get(int64_t id)
{
    return OrganizationApplyRepository::Get(id, GetDB());
}

int64_t OrganizationApplyManager::listByPages(std::vector<DB::OrganizationApplyInfo::ptr>& results,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool is_valid)
{
    return OrganizationApplyRepository::ListByPages(results, offset, limit, status, is_valid, GetDB());
}

int64_t OrganizationApplyManager::listByUserId(std::vector<DB::OrganizationApplyInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t limit,
    bool is_valid)
{
    return OrganizationApplyRepository::ListByUser(results, user_id, offset, limit, is_valid, GetDB());
}
