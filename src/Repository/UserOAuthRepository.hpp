/**
 * @file UserOAuthRepository.hpp
 * @brief 第三方登录绑定表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserOauthInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>

class UserOAuthRepository {
public:
    static DB::UserOauthInfo::ptr GetByOpenId(const std::string& provider,
        const std::string& open_id,
        chen::IDB::ptr conn);

    static DB::UserOauthInfo::ptr GetByUser(int64_t user_id, const std::string& provider, chen::IDB::ptr conn);

    static bool Insert(DB::UserOauthInfo::ptr info, chen::IDB::ptr conn);
};
