#include "RequirementManager.hpp"

#include "../Repository/RequirementRepository.hpp"
#include "../Util/Util.hpp"

DB::RequirementInfo::ptr RequirementManager::get(int64_t id)
{
    return RequirementRepository::Get(id, GetDB());
}

uint64_t RequirementManager::listByPages(std::vector<DB::RequirementInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return RequirementRepository::ListByPages(infos, offset, size, status, is_valid, GetDB());
}

uint64_t RequirementManager::listByOrg(std::vector<DB::RequirementInfo::ptr>& infos,
    int64_t org_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return RequirementRepository::ListByOrg(infos, org_id, offset, size, status, is_valid, GetDB());
}
