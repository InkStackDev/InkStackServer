#include "UserAIConfigService.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/UserAIConfigRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

bool UserAIConfigService::Save(DB::UserAiConfigInfo::ptr info)
{
    if (!info) {
        return false;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setUpdateTime(now);
    if (!info->getId()) {
        info->setCreateTime(now);
    }

    if (!UserAIConfigRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "UserAIConfigService::Save failed, user_id=" << info->getUserId();
        return false;
    }

    // InsertOrUpdate 后绕过缓存按 user_id 回查，确保拿到 DB 生成的主键
    auto saved = UserAIConfigRepository::ReloadByUserId(info->getUserId(), db);
    if (saved) {
        UserAIConfigRepository::Put(saved);
    }
    return true;
}
