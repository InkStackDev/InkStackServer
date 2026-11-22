#include "AssignmentManager.hpp"

#include "../Repository/AssignmentQueryRepository.hpp"
#include "../Repository/AssignmentRepository.hpp"
#include "../Util/Util.hpp"

void AssignmentManager::add(DB::AssignmentInfo::ptr info)
{
    AssignmentRepository::Put(info);
}

DB::AssignmentInfo::ptr AssignmentManager::get(int64_t id)
{
    return AssignmentRepository::Get(id, GetDB());
}

DB::AssignmentInfo::ptr AssignmentManager::getByName(const std::string& subject_name, const std::string& name)
{
    return AssignmentRepository::GetByName(subject_name, name, GetDB());
}

uint64_t AssignmentManager::listByPages(std::vector<DB::AssignmentInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool is_valid)
{
    return AssignmentQueryRepository::ListByPages(infos, offset, size, status, is_valid, GetDB());
}

AssignmentManager::AssignmentStats AssignmentManager::getStats()
{
    return AssignmentQueryRepository::GetStats(GetDB());
}
