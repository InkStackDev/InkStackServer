/**
 * @file UserService.hpp
 * @brief 用户用例：资料修改、2FA 关闭等需要事务与缓存一致性的操作
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserInfo.hpp"

#include <cstdint>
#include <string>

/**
 * @brief 用户业务服务
 * @details Service 负责业务校验、事务边界和 commit 后的缓存失效；
 *          Repository 只做单表读写。
 */
class UserService {
public:
    enum class UpdateProfileResult {
        Ok,
        UserNotFound,
        OldPasswordRequired,
        OldPasswordWrong,
        InvalidPassword,
        DbError,
        InternalError
    };

    struct ProfileUpdate {
        std::string name;
        std::string passwd;
        std::string bio;
        std::string website;
        std::string github;
        std::string avatar;
        std::string old_passwd;
    };

    enum class Disable2faResult {
        Ok,
        UserNotFound,
        NotEnabled,
        InvalidCode,
        DbError
    };

    static DB::UserInfo::ptr Get(int64_t uid);

    /**
     * @brief 修改个人资料
     * @param[out] password_changed 本次是否修改了密码（调用方据此撤销会话）
     */
    static UpdateProfileResult UpdateProfile(int64_t uid, const ProfileUpdate& input, bool& password_changed);

    /**
     * @brief 关闭 TOTP 2FA，并清理恢复码
     */
    static Disable2faResult Disable2FA(int64_t uid, const std::string& code);
};
