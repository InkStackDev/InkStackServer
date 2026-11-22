#include "BugManager.hpp"

#include "../Repository/BugRepository.hpp"
#include "../Util/Util.hpp"

DB::BugInfo::ptr BugManager::get(int64_t id)
{
    return BugRepository::Get(id, GetDB());
}

uint64_t BugManager::listByPages(std::vector<DB::BugInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return BugRepository::ListByPages(infos, offset, size, status, is_valid, GetDB());
}

uint64_t BugManager::listByOrg(std::vector<DB::BugInfo::ptr>& infos,
    int64_t org_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return BugRepository::ListByOrg(infos, org_id, offset, size, status, is_valid, GetDB());
}
