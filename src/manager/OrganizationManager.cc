#include "OrganizationManager.hpp"

#include "../Repository/OrganizationRepository.hpp"
#include "../Util/Util.hpp"

void OrganizationManager::add(DB::OrganizationInfo::ptr info)
{
    OrganizationRepository::Put(info);
}

DB::OrganizationInfo::ptr OrganizationManager::get(int64_t id)
{
    return OrganizationRepository::Get(id, GetDB());
}

DB::OrganizationInfo::ptr OrganizationManager::getByName(const std::string& name)
{
    return OrganizationRepository::GetByName(name, GetDB());
}

int64_t OrganizationManager::listByPages(std::vector<DB::OrganizationInfo::ptr>& orgs,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool is_valid)
{
    return OrganizationQueryRepository::ListByPages(orgs, offset, limit, status, is_valid, GetDB());
}

OrganizationManager::OrganizationStats OrganizationManager::getStats()
{
    return OrganizationQueryRepository::GetStats(GetDB());
}
