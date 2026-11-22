#include "AssignmentUserRelManager.hpp"

#include "../Repository/AssignmentRelRepository.hpp"
#include "../Util/Util.hpp"

DB::AssignmentUserRelInfo::ptr AssignmentUserRelManager::get(int64_t id)
{
    return AssignmentUserRelRepository::Get(id, GetDB());
}

DB::AssignmentUserRelInfo::ptr AssignmentUserRelManager::getByAssignAndUser(int64_t assign_id, int64_t user_id)
{
    return AssignmentUserRelRepository::GetByAssignAndUser(assign_id, user_id, GetDB());
}
