#include "CategoryManager.hpp"

#include "../Repository/CategoryRepository.hpp"
#include "../Util/Util.hpp"

void CategoryManager::add(DB::CategoryInfo::ptr info)
{
    CategoryRepository::Put(info);
}

DB::CategoryInfo::ptr CategoryManager::get(int64_t id)
{
    return CategoryRepository::Get(id, GetDB());
}

void CategoryManager::listAll(std::vector<DB::CategoryInfo::ptr>& infos, bool is_valid)
{
    CategoryRepository::ListAll(infos, is_valid, GetDB());
}

bool CategoryManager::listByUserId(std::vector<DB::CategoryInfo::ptr>& infos, int64_t user_id, bool is_valid)
{
    return CategoryRepository::ListByUserId(infos, user_id, is_valid, GetDB());
}

DB::CategoryInfo::ptr CategoryManager::getByUserIdName(int64_t user_id, const std::string& name)
{
    return CategoryRepository::GetByUserIdName(user_id, name, GetDB());
}
