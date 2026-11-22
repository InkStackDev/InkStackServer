#include "TaskManager.hpp"

#include "../Repository/TaskRepository.hpp"
#include "../Util/Util.hpp"

DB::TaskInfo::ptr TaskManager::get(int64_t id)
{
    return TaskRepository::Get(id, GetDB());
}

uint64_t TaskManager::listByPages(std::vector<DB::TaskInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return TaskRepository::ListByPages(infos, offset, size, status, is_valid, GetDB());
}

uint64_t TaskManager::listByOrg(std::vector<DB::TaskInfo::ptr>& infos,
    int64_t org_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return TaskRepository::ListByOrg(infos, org_id, offset, size, status, is_valid, GetDB());
}
