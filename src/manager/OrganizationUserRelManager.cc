#include "OrganizationUserRelManager.hpp"

#include "../Repository/OrganizationUserRelRepository.hpp"
#include "../Util/Util.hpp"

void OrganizationUserRelManager::add(DB::OrganizationUserRelInfo::ptr info)
{
    OrganizationUserRelRepository::Put(info);
}

DB::OrganizationUserRelInfo::ptr OrganizationUserRelManager::get(int64_t id)
{
    return OrganizationUserRelRepository::Get(id, GetDB());
}

DB::OrganizationUserRelInfo::ptr OrganizationUserRelManager::getByOrgAndUser(int64_t o_id, int64_t u_id)
{
    return OrganizationUserRelRepository::GetByOrgAndUser(o_id, u_id, GetDB());
}

int64_t OrganizationUserRelManager::getByPages(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
    int64_t o_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return OrganizationUserRelRepository::ListByOrgPages(results, o_id, offset, size, status, is_valid, GetDB());
}

int64_t OrganizationUserRelManager::getOrgByUserId(std::vector<DB::OrganizationUserRelInfo::ptr>& results,
    int64_t u_id,
    int32_t status,
    bool is_valid)
{
    return OrganizationUserRelRepository::ListByUser(results, u_id, status, is_valid, GetDB());
}

int64_t OrganizationUserRelManager::getMemberCount(int64_t o_id, int32_t status, bool is_valid)
{
    return OrganizationUserRelRepository::CountByOrg(o_id, status, is_valid, GetDB());
}

OrganizationUserRelManager::Stats OrganizationUserRelManager::getStats(int64_t org_id)
{
    return OrganizationUserRelRepository::GetStats(org_id, GetDB());
}
