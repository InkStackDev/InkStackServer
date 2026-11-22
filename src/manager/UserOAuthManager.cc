#include "UserOAuthManager.hpp"

#include <memory>

#include "../Repository/UserOAuthRepository.hpp"
#include "../Util/Util.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

UserOAuthManager::UserOAuthManager()
{
}

DB::UserOauthInfo::ptr UserOAuthManager::getByOpenId(const std::string& provider, const std::string& open_id)
{
    if (provider.empty() || open_id.empty()) {
        return nullptr;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getByOpenId: GetDB failed";
        return nullptr;
    }
    return UserOAuthRepository::GetByOpenId(provider, open_id, db);
}

DB::UserOauthInfo::ptr UserOAuthManager::getByUser(int64_t user_id, const std::string& provider)
{
    if (!user_id || provider.empty()) {
        return nullptr;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getByUser: GetDB failed";
        return nullptr;
    }
    return UserOAuthRepository::GetByUser(user_id, provider, db);
}

int UserOAuthManager::bind(int64_t user_id, const std::string& provider, const std::string& open_id, const std::string& login_name)
{
    if (!user_id || provider.empty() || open_id.empty()) {
        return -1;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "bind: GetDB failed";
        return -1;
    }
    auto info = std::make_shared<DB::UserOauthInfo>();
    info->setUserId(user_id);
    info->setProvider(provider);
    info->setOpenId(open_id);
    info->setLoginName(login_name);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    info->setCreateTime(now);
    info->setUpdateTime(now);
    if (!UserOAuthRepository::Insert(info, db)) {
        ERROR(logger) << "bind: Insert failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return -1;
    }
    return 0;
}
