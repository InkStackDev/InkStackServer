/**
 * @file UserOAuthService.hpp
 * @brief 第三方登录绑定用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserOauthInfo.hpp"

#include <cstdint>
#include <string>

class UserOAuthService {
public:
    static DB::UserOauthInfo::ptr GetByOpenId(const std::string& provider, const std::string& open_id);

    /**
     * @brief 绑定第三方账号到本地用户
     * @return 0 成功，负数失败
     */
    static int Bind(int64_t user_id,
        const std::string& provider,
        const std::string& open_id,
        const std::string& login_name);
};
