#include "UserAIConfigManager.hpp"

#include "../Repository/UserAIConfigRepository.hpp"
#include "../Service/UserAIConfigService.hpp"
#include "../Util/Util.hpp"

DB::UserAiConfigInfo::ptr UserAIConfigManager::getByUserId(int64_t user_id)
{
    return UserAIConfigRepository::GetByUserId(user_id, GetDB());
}

bool UserAIConfigManager::save(DB::UserAiConfigInfo::ptr info)
{
    return UserAIConfigService::Save(info);
}
