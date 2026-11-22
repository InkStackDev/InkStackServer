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
#include <vector>
#include "../Domain/UserTypes.hpp"

/**
 * @brief 用户业务服务
 * @details Service 负责业务校验、事务边界和 commit 后的缓存失效；
 *          Repository 只做单表读写。
 */
class UserService {
public:
    /// 生成一次性随机 token（OAuth state / 2FA 临时凭证）
    static std::string GenerateToken();

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

    enum class SetPasswordResult {
        Ok,
        UserNotFound,
        InvalidPassword,
        InternalError,
        DbError
    };

    enum class DeleteUserResult {
        Ok,
        UserNotFound,
        AlreadyDeleted,
        DbError
    };

    struct AdminUpdateInput {
        std::string account;
        std::string email;
        std::string passwd;
        int32_t role = 0;
        int32_t state = 0;
        bool has_role = false;
        bool has_state = false;
    };

    enum class AdminUpdateResult {
        Ok,
        UserNotFound,
        NoParam,
        InvalidAccount,
        AccountExists,
        InvalidEmail,
        EmailExists,
        InvalidPassword,
        InvalidRole,
        InvalidState,
        InternalError,
        DbError
    };

    struct CreateUserInput {
        std::string account;
        std::string email;
        std::string passwd;
        int32_t role = 1;  // UserRole::USER
        int32_t state = 1; // UserState::ACTIVE
    };

    enum class CreateUserResult {
        Ok,
        AccountExists,
        EmailExists,
        InvalidAccount,
        InvalidEmail,
        InvalidPassword,
        InvalidRole,
        InvalidState,
        InternalError,
        DbError
    };

    enum class Begin2faResult {
        Ok,
        UserNotFound,
        AlreadyEnabled
    };

    enum class Confirm2faResult {
        Ok,
        UserNotFound,
        AlreadyEnabled,
        SecretMissing,
        InvalidCode,
        DbError
    };

    enum class Reset2faResult {
        Ok,
        UserNotFound,
        NotEnabled,
        InvalidCode,
        DbError
    };

    enum class ConsumeRecoveryResult {
        Ok,
        UserNotFound,
        AccountInvalid,
        NotEnabled,
        InvalidCode,
        DbError
    };

    struct OAuthUserInput {
        std::string open_id;
        std::string login;
        std::string name;
        std::string email;
        std::string avatar;
    };

    enum class FindOrCreateOAuthResult {
        Ok,
        InternalError,
        DbError
    };

    static DB::UserInfo::ptr Get(int64_t uid);

    static DB::UserInfo::ptr GetByEmail(const std::string& email);

    static DB::UserInfo::ptr GetByAccount(const std::string& account);

    static bool UpgradePasswordHash(int64_t uid, const std::string& old_hash, const std::string& new_hash);

    /**
     * @brief 修改个人资料
     * @param[out] password_changed 本次是否修改了密码（调用方据此撤销会话）
     */
    static UpdateProfileResult UpdateProfile(int64_t uid, const ProfileUpdate& input, bool& password_changed);

    /**
     * @brief 关闭 TOTP 2FA，并清理恢复码
     */
    static Disable2faResult Disable2FA(int64_t uid, const std::string& code);

    /**
     * @brief 设置密码（管理员重置 / 邮箱找回共用）
     */
    static SetPasswordResult SetPassword(int64_t uid, const std::string& plain, DB::UserInfo::ptr& out);

    static SetPasswordResult SetPasswordByEmail(const std::string& email,
        const std::string& plain,
        DB::UserInfo::ptr& out);

    /**
     * @brief 软删除用户
     */
    static DeleteUserResult DeleteUser(int64_t uid);

    /**
     * @brief 批量恢复已删除用户
     */
    static bool RecoverUsers(const std::vector<int64_t>& ids, std::vector<int64_t>& recovered);

    /**
     * @brief 管理端修改用户资料/角色/状态
     * @param[out] revoke_sessions 是否需要撤销该用户全部会话
     */
    static AdminUpdateResult AdminUpdate(int64_t uid,
        const AdminUpdateInput& input,
        DB::UserInfo::ptr& out,
        bool& revoke_sessions);

    /**
     * @brief 创建用户（注册 / 管理端创建共用）
     */
    static CreateUserResult CreateUser(const CreateUserInput& input, DB::UserInfo::ptr& out);

    /**
     * @brief 生成待确认的 2FA 密钥（不落库）
     */
    static Begin2faResult BeginEnable2FA(int64_t uid, std::string& secret, std::string& otp_uri);

    /**
     * @brief 确认启用 2FA，并生成恢复码
     */
    static Confirm2faResult ConfirmEnable2FA(int64_t uid,
        const std::string& secret,
        const std::string& code,
        std::vector<std::string>& recovery_codes);

    /**
     * @brief 使用恢复码关闭 2FA，并生成新的待确认密钥
     */
    static Reset2faResult Reset2FA(int64_t uid,
        const std::string& recovery_code,
        std::string& new_secret,
        std::string& new_otp_uri);

    /**
     * @brief 使用恢复码完成 2FA 登录（只消费恢复码）
     */
    static ConsumeRecoveryResult ConsumeRecoveryCode(int64_t uid, const std::string& recovery_code);

    /**
     * @brief 查找或创建 GitHub OAuth 用户
     * @details 邮箱已存在时直接返回已有用户；否则生成唯一账号/用户名并创建。
     */
    static FindOrCreateOAuthResult FindOrCreateOAuthUser(const OAuthUserInput& input, DB::UserInfo::ptr& out);
};
