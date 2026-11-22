#include "LabelManager.hpp"

#include "../Repository/LabelRepository.hpp"
#include "../Util/Util.hpp"

void LabelManager::add(DB::LabelInfo::ptr info)
{
    LabelRepository::Put(info);
}

DB::LabelInfo::ptr LabelManager::get(int64_t id)
{
    return LabelRepository::Get(id, GetDB());
}

DB::LabelInfo::ptr LabelManager::getByUserIdName(int64_t id, const std::string& name)
{
    return LabelRepository::GetByUserIdName(id, name, GetDB());
}

bool LabelManager::listByUserId(std::vector<DB::LabelInfo::ptr>& infos, int64_t id, bool valid)
{
    return LabelRepository::ListByUserId(infos, id, valid, GetDB());
}
