#include "UserOAuthService.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Repository/UserOAuthRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::UserOauthInfo::ptr UserOAuthService::GetByOpenId(const std::string& provider, const std::string& open_id)
{
    return UserOAuthRepository::GetByOpenId(provider, open_id, GetDB());
}

int UserOAuthService::Bind(int64_t user_id,
    const std::string& provider,
    const std::string& open_id,
    const std::string& login_name)
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
