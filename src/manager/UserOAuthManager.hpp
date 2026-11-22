/**
 * @file UserOAuthManager.hpp
 * @brief 第三方登录绑定管理器 — 按 provider + open_id 查询与绑定
 * @author Christins
 * @date 2026-10-11
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserOauthInfo.hpp"

#include <Chen/DB/QueryBuilder.hpp>
#include <Chen/Util/Singleton.hpp>

class UserOAuthManager {
public:
    UserOAuthManager();

    /**
     * @brief 按第三方平台唯一标识查询绑定记录
     * @return 绑定记录，不存在返回 nullptr
     */
    DB::UserOauthInfo::ptr getByOpenId(const std::string& provider, const std::string& open_id);

    /**
     * @brief 查询某用户在某平台的绑定记录
     * @return 绑定记录，不存在返回 nullptr
     */
    DB::UserOauthInfo::ptr getByUser(int64_t user_id, const std::string& provider);

    /**
     * @brief 绑定第三方账号到本地用户
     * @return 0 成功，负数失败
     */
    int bind(int64_t user_id, const std::string& provider, const std::string& open_id, const std::string& login_name);
};

typedef chen::Singleton<UserOAuthManager> UserOAuthMgr;
