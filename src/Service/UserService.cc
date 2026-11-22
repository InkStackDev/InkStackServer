#include "UserService.hpp"

#include <memory>
#include <vector>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/EncryptorUtil.hpp>
#include <Chen/Util/RandomUtil.hpp>
#include <Chen/Util/StringUtil.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "DB/UserRecoveryCodeInfo.hpp"

#include "../Repository/UserRecoveryCodeRepository.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/TotpUtil.hpp"
#include "../Util/Util.hpp"
#include "../Domain/UserTypes.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

std::string UserService::GenerateToken()
{
    return chen::EncryptorUtil::MD5(chen::RandomUtil::RandString(32));
}

namespace {

std::string BuildOAuthAccount(chen::IDB::ptr db, const std::string& open_id, const std::string& login)
{
    if (IsValidAccount(login) && !UserRepository::GetByAccount(login, db)) {
        return login;
    }
    std::string base = "gh_" + open_id;
    if (!IsValidAccount(base)) {
        base = "gh" + open_id;
    }
    if (base.size() > 16) {
        base = base.substr(0, 16);
    }
    if (!IsValidAccount(base)) {
        base = "ghuser";
    }

    std::string candidate = base;
    int suffix = 1;
    while (UserRepository::GetByAccount(candidate, db)) {
        std::string s = std::to_string(suffix++);
        std::string prefix = base;
        if (prefix.size() + s.size() > 16) {
            prefix = prefix.substr(0, 16 - s.size());
        }
        candidate = prefix + s;
    }
    return candidate;
}

std::string BuildOAuthName(chen::IDB::ptr db,
    const std::string& open_id,
    const std::string& uname,
    const std::string& login)
{
    std::string base = !uname.empty() ? uname : login;
    base = chen::StringUtil::Trim(base);
    if (base.empty()) {
        base = "github_" + open_id;
    }
    if (!UserRepository::GetByName(base, db)) {
        return base;
    }

    std::string candidate = base + "_" + open_id;
    int suffix = 1;
    while (UserRepository::GetByName(candidate, db)) {
        candidate = base + "_" + open_id + "_" + std::to_string(suffix++);
    }
    return candidate;
}

} // namespace

DB::UserInfo::ptr UserService::Get(int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }
    return UserRepository::Get(uid, db);
}

DB::UserInfo::ptr UserService::GetByEmail(const std::string& email)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }
    return UserRepository::GetByEmail(email, db);
}

DB::UserInfo::ptr UserService::GetByAccount(const std::string& account)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }
    return UserRepository::GetByAccount(account, db);
}

bool UserService::UpgradePasswordHash(int64_t uid, const std::string& old_hash, const std::string& new_hash)
{
    if (uid <= 0 || new_hash.empty()) {
        return false;
    }
    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info || info->getPasswd() != old_hash) {
        return false;
    }
    info->setPasswd(new_hash);
    if (!UserRepository::Update(info, db)) {
        return false;
    }
    UserRepository::Put(info);
    return true;
}

UserService::UpdateProfileResult UserService::UpdateProfile(int64_t uid,
    const ProfileUpdate& input,
    bool& password_changed)
{
    password_changed = false;
    if (uid <= 0) {
        return UpdateProfileResult::UserNotFound;
    }
    if (!input.passwd.empty() && input.passwd.size() < 6) {
        return UpdateProfileResult::InvalidPassword;
    }

    auto db = GetDB();
    if (!db) {
        return UpdateProfileResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return UpdateProfileResult::UserNotFound;
    }

    if (!input.passwd.empty()) {
        if (input.old_passwd.empty()) {
            return UpdateProfileResult::OldPasswordRequired;
        }
        if (!VerifyPassword(input.old_passwd, info->getPasswd())) {
            return UpdateProfileResult::OldPasswordWrong;
        }
        std::string passwd_hash = HashPassword(input.passwd);
        if (passwd_hash.empty()) {
            return UpdateProfileResult::InternalError;
        }
        info->setPasswd(passwd_hash);
    }

    if (!input.name.empty()) {
        info->setName(input.name);
    }
    if (!input.bio.empty()) {
        info->setBio(input.bio);
    }
    if (!input.website.empty()) {
        info->setWebsite(input.website);
    }
    if (!input.github.empty()) {
        info->setGithub(input.github);
    }
    if (!input.avatar.empty()) {
        info->setAvatar(input.avatar);
    }
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!UserRepository::Update(info, db)) {
        ERROR(logger) << "UpdateProfile failed, uid=" << uid << " errno=" << db->getErrno()
                      << " errstr=" << db->getErrStr();
        return UpdateProfileResult::DbError;
    }

    UserRepository::Put(info);
    password_changed = !input.passwd.empty();
    return UpdateProfileResult::Ok;
}

UserService::Disable2faResult UserService::Disable2FA(int64_t uid, const std::string& code)
{
    if (uid <= 0) {
        return Disable2faResult::UserNotFound;
    }

    auto db = GetDB();
    if (!db) {
        return Disable2faResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return Disable2faResult::UserNotFound;
    }
    if (!info->getTotpEnabled()) {
        return Disable2faResult::NotEnabled;
    }
    if (code.empty() || !TotpUtil::VerifyCode(info->getTotpSecret(), code, 1)) {
        return Disable2faResult::InvalidCode;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        ERROR(logger) << "Disable2FA: open transaction failed, uid=" << uid;
        return Disable2faResult::DbError;
    }

    info->setTotpEnabled(0);
    info->setTotpSecret("");
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!UserRepository::Update(info, db)) {
        ERROR(logger) << "Disable2FA: update user failed, uid=" << uid << " errno=" << db->getErrno()
                      << " errstr=" << db->getErrStr();
        return Disable2faResult::DbError;
    }

    if (!UserRecoveryCodeRepository::DeleteByUser(uid, db)) {
        ERROR(logger) << "Disable2FA: clear recovery codes failed, uid=" << uid;
        return Disable2faResult::DbError;
    }

    if (!trans->commit()) {
        ERROR(logger) << "Disable2FA: commit failed, uid=" << uid;
        return Disable2faResult::DbError;
    }

    // 事务提交后再更新缓存，避免回滚时把未落库状态写进缓存
    UserRepository::Put(info);
    return Disable2faResult::Ok;
}

UserService::SetPasswordResult UserService::SetPassword(int64_t uid, const std::string& plain, DB::UserInfo::ptr& out)
{
    out.reset();
    if (plain.size() < 6) {
        return SetPasswordResult::InvalidPassword;
    }

    auto db = GetDB();
    if (!db) {
        return SetPasswordResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return SetPasswordResult::UserNotFound;
    }

    std::string passwd_hash = HashPassword(plain);
    if (passwd_hash.empty()) {
        return SetPasswordResult::InternalError;
    }
    info->setPasswd(passwd_hash);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());

    if (!UserRepository::Update(info, db)) {
        ERROR(logger) << "SetPassword failed, uid=" << uid << " errno=" << db->getErrno()
                      << " errstr=" << db->getErrStr();
        return SetPasswordResult::DbError;
    }
    UserRepository::Put(info);
    out = info;
    return SetPasswordResult::Ok;
}

UserService::SetPasswordResult UserService::SetPasswordByEmail(const std::string& email,
    const std::string& plain,
    DB::UserInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return SetPasswordResult::DbError;
    }
    auto info = UserRepository::GetByEmail(email, db);
    if (!info) {
        return SetPasswordResult::UserNotFound;
    }
    return SetPassword(info->getId(), plain, out);
}

UserService::DeleteUserResult UserService::DeleteUser(int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        return DeleteUserResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return DeleteUserResult::UserNotFound;
    }
    if (info->getIsDeleted()) {
        return DeleteUserResult::AlreadyDeleted;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return DeleteUserResult::DbError;
    }
    info->setIsDeleted(1);
    info->setState(UserState::INACTIVE);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!UserRepository::Update(info, db)) {
        ERROR(logger) << "DeleteUser update failed, uid=" << uid;
        return DeleteUserResult::DbError;
    }
    if (!trans->commit()) {
        ERROR(logger) << "DeleteUser commit failed, uid=" << uid;
        return DeleteUserResult::DbError;
    }
    UserRepository::Put(info);
    return DeleteUserResult::Ok;
}

bool UserService::RecoverUsers(const std::vector<int64_t>& ids, std::vector<int64_t>& recovered)
{
    recovered.clear();
    if (ids.empty()) {
        return true;
    }

    auto db = GetDB();
    if (!db) {
        return false;
    }
    auto trans = db->openTransaction();
    if (!trans) {
        return false;
    }

    std::vector<DB::UserInfo::ptr> updated;
    for (auto id : ids) {
        auto info = UserRepository::Get(id, db);
        if (!info || !info->getIsDeleted()) {
            continue;
        }
        info->setIsDeleted(0);
        info->setState(UserState::ACTIVE);
        info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
        if (!UserRepository::Update(info, db)) {
            ERROR(logger) << "RecoverUsers update failed, uid=" << id;
            return false;
        }
        updated.push_back(info);
    }

    if (!trans->commit()) {
        ERROR(logger) << "RecoverUsers commit failed";
        return false;
    }
    for (const auto& info : updated) {
        UserRepository::Put(info);
        recovered.push_back(info->getId());
    }
    return true;
}

UserService::AdminUpdateResult UserService::AdminUpdate(int64_t uid,
    const AdminUpdateInput& input,
    DB::UserInfo::ptr& out,
    bool& revoke_sessions)
{
    out.reset();
    revoke_sessions = false;

    auto db = GetDB();
    if (!db) {
        return AdminUpdateResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return AdminUpdateResult::UserNotFound;
    }
    if (input.account.empty() && input.email.empty() && input.passwd.empty() && !input.has_role && !input.has_state) {
        return AdminUpdateResult::NoParam;
    }

    if (!input.account.empty()) {
        if (!IsValidAccount(input.account)) {
            return AdminUpdateResult::InvalidAccount;
        }
        auto exist = UserRepository::GetByAccount(input.account, db);
        if (exist && exist->getId() != uid) {
            return AdminUpdateResult::AccountExists;
        }
        info->setAccount(input.account);
    }
    if (!input.email.empty()) {
        if (!IsEmail(input.email)) {
            return AdminUpdateResult::InvalidEmail;
        }
        auto exist = UserRepository::GetByEmail(input.email, db);
        if (exist && exist->getId() != uid) {
            return AdminUpdateResult::EmailExists;
        }
        info->setEmail(input.email);
    }
    if (!input.passwd.empty()) {
        if (input.passwd.size() < 6) {
            return AdminUpdateResult::InvalidPassword;
        }
        std::string passwd_hash = HashPassword(input.passwd);
        if (passwd_hash.empty()) {
            return AdminUpdateResult::InternalError;
        }
        info->setPasswd(passwd_hash);
    }
    if (input.has_role) {
        if (input.role < 1 || input.role > 4) {
            return AdminUpdateResult::InvalidRole;
        }
        info->setRole(input.role);
    }
    if (input.has_state) {
        if (input.state < 1 || input.state > 2) {
            return AdminUpdateResult::InvalidState;
        }
        info->setState(input.state);
    }

    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!UserRepository::Update(info, db)) {
        ERROR(logger) << "AdminUpdate failed, uid=" << uid << " errno=" << db->getErrno()
                      << " errstr=" << db->getErrStr();
        return AdminUpdateResult::DbError;
    }
    UserRepository::Put(info);
    out = info;
    revoke_sessions = !input.passwd.empty() || (input.has_state && input.state != UserState::ACTIVE);
    return AdminUpdateResult::Ok;
}

UserService::CreateUserResult UserService::CreateUser(const CreateUserInput& input, DB::UserInfo::ptr& out)
{
    out.reset();
    if (input.account.empty() || !IsValidAccount(input.account)) {
        return CreateUserResult::InvalidAccount;
    }
    if (!IsEmail(input.email)) {
        return CreateUserResult::InvalidEmail;
    }
    if (input.passwd.size() < 6) {
        return CreateUserResult::InvalidPassword;
    }
    if (input.role < 1 || input.role > 4) {
        return CreateUserResult::InvalidRole;
    }
    if (input.state < 1 || input.state > 2) {
        return CreateUserResult::InvalidState;
    }

    auto db = GetDB();
    if (!db) {
        return CreateUserResult::DbError;
    }
    if (UserRepository::GetByAccount(input.account, db)) {
        return CreateUserResult::AccountExists;
    }
    if (UserRepository::GetByEmail(input.email, db)) {
        return CreateUserResult::EmailExists;
    }

    std::string passwd_hash = HashPassword(input.passwd);
    if (passwd_hash.empty()) {
        return CreateUserResult::InternalError;
    }

    auto info = std::make_shared<DB::UserInfo>();
    info->setAccount(input.account);
    info->setEmail(input.email);
    info->setPasswd(passwd_hash);
    info->setRole(input.role);
    info->setState(input.state);
    info->setName(input.account);

    if (!UserRepository::Insert(info, db)) {
        ERROR(logger) << "CreateUser insert failed, account=" << input.account << " errno=" << db->getErrno()
                      << " errstr=" << db->getErrStr();
        return CreateUserResult::DbError;
    }
    UserRepository::Put(info);
    out = info;
    return CreateUserResult::Ok;
}

UserService::Begin2faResult UserService::BeginEnable2FA(int64_t uid, std::string& secret, std::string& otp_uri)
{
    secret.clear();
    otp_uri.clear();

    auto db = GetDB();
    if (!db) {
        return Begin2faResult::UserNotFound;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return Begin2faResult::UserNotFound;
    }
    if (info->getTotpEnabled()) {
        return Begin2faResult::AlreadyEnabled;
    }

    secret = TotpUtil::GenerateSecret();
    otp_uri = TotpUtil::GetOtpUri(secret, info->getAccount());
    return Begin2faResult::Ok;
}

UserService::Confirm2faResult UserService::ConfirmEnable2FA(int64_t uid,
    const std::string& secret,
    const std::string& code,
    std::vector<std::string>& recovery_codes)
{
    recovery_codes.clear();
    if (secret.empty()) {
        return Confirm2faResult::SecretMissing;
    }

    auto db = GetDB();
    if (!db) {
        return Confirm2faResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return Confirm2faResult::UserNotFound;
    }
    if (info->getTotpEnabled()) {
        return Confirm2faResult::AlreadyEnabled;
    }
    if (!TotpUtil::VerifyCode(secret, code, 1)) {
        return Confirm2faResult::InvalidCode;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return Confirm2faResult::DbError;
    }

    info->setTotpSecret(secret);
    info->setTotpEnabled(1);
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!UserRepository::Update(info, db)) {
        ERROR(logger) << "ConfirmEnable2FA update failed, uid=" << uid;
        return Confirm2faResult::DbError;
    }

    // 清理历史恢复码，避免重复绑定后旧码仍然可用
    if (!UserRecoveryCodeRepository::DeleteByUser(uid, db)) {
        ERROR(logger) << "ConfirmEnable2FA clear old recovery codes failed, uid=" << uid;
        return Confirm2faResult::DbError;
    }

    std::vector<std::string> hashed_codes;
    recovery_codes = TotpUtil::GenerateRecoveryCodes(10, hashed_codes);
    for (const auto& hash : hashed_codes) {
        auto rc = std::make_shared<DB::UserRecoveryCodeInfo>();
        rc->setUserId(uid);
        rc->setCodeHash(hash);
        rc->setUsed(0);
        if (UserRecoveryCodeRepository::Insert(rc, db)) {
            ERROR(logger) << "ConfirmEnable2FA insert recovery code failed, uid=" << uid;
            return Confirm2faResult::DbError;
        }
    }

    if (!trans->commit()) {
        ERROR(logger) << "ConfirmEnable2FA commit failed, uid=" << uid;
        return Confirm2faResult::DbError;
    }
    UserRepository::Put(info);
    return Confirm2faResult::Ok;
}

UserService::Reset2faResult UserService::Reset2FA(int64_t uid,
    const std::string& recovery_code,
    std::string& new_secret,
    std::string& new_otp_uri)
{
    new_secret.clear();
    new_otp_uri.clear();

    auto db = GetDB();
    if (!db) {
        return Reset2faResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return Reset2faResult::UserNotFound;
    }
    if (!info->getTotpEnabled()) {
        return Reset2faResult::NotEnabled;
    }

    auto matched = UserRecoveryCodeRepository::FindUnused(uid, TotpUtil::HashRecoveryCode(recovery_code), db);
    if (!matched) {
        return Reset2faResult::InvalidCode;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return Reset2faResult::DbError;
    }

    info->setTotpEnabled(0);
    info->setTotpSecret("");
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (!UserRepository::Update(info, db)) {
        ERROR(logger) << "Reset2FA update user failed, uid=" << uid;
        return Reset2faResult::DbError;
    }

    // 旧恢复码已全部失效，直接清空，避免重新绑定后继续可用
    if (!UserRecoveryCodeRepository::DeleteByUser(uid, db)) {
        ERROR(logger) << "Reset2FA clear recovery codes failed, uid=" << uid;
        return Reset2faResult::DbError;
    }

    if (!trans->commit()) {
        ERROR(logger) << "Reset2FA commit failed, uid=" << uid;
        return Reset2faResult::DbError;
    }
    UserRepository::Put(info);

    new_secret = TotpUtil::GenerateSecret();
    new_otp_uri = TotpUtil::GetOtpUri(new_secret, info->getAccount());
    return Reset2faResult::Ok;
}

UserService::ConsumeRecoveryResult UserService::ConsumeRecoveryCode(int64_t uid, const std::string& recovery_code)
{
    auto db = GetDB();
    if (!db) {
        return ConsumeRecoveryResult::DbError;
    }
    auto info = UserRepository::Get(uid, db);
    if (!info) {
        return ConsumeRecoveryResult::UserNotFound;
    }
    if (info->getState() != UserState::ACTIVE || info->getIsDeleted()) {
        return ConsumeRecoveryResult::AccountInvalid;
    }
    if (!info->getTotpEnabled()) {
        return ConsumeRecoveryResult::NotEnabled;
    }

    auto matched = UserRecoveryCodeRepository::FindUnused(uid, TotpUtil::HashRecoveryCode(recovery_code), db);
    if (!matched) {
        return ConsumeRecoveryResult::InvalidCode;
    }

    auto trans = db->openTransaction();
    if (!trans) {
        return ConsumeRecoveryResult::DbError;
    }
    matched->setUsed(1);
    if (UserRecoveryCodeRepository::Update(matched, db)) {
        ERROR(logger) << "ConsumeRecoveryCode update failed, uid=" << uid;
        return ConsumeRecoveryResult::DbError;
    }
    if (!trans->commit()) {
        ERROR(logger) << "ConsumeRecoveryCode commit failed, uid=" << uid;
        return ConsumeRecoveryResult::DbError;
    }
    return ConsumeRecoveryResult::Ok;
}

UserService::FindOrCreateOAuthResult UserService::FindOrCreateOAuthUser(const OAuthUserInput& input,
    DB::UserInfo::ptr& out)
{
    out.reset();
    auto db = GetDB();
    if (!db) {
        return FindOrCreateOAuthResult::DbError;
    }

    std::string email = input.email;
    if (email.empty()) {
        email = input.open_id + "+" + input.login + "@users.noreply.github.com";
    }
    auto existing = UserRepository::GetByEmail(email, db);
    if (existing) {
        out = existing;
        return FindOrCreateOAuthResult::Ok;
    }

    std::string passwd_hash = HashPassword(UserService::GenerateToken());
    if (passwd_hash.empty()) {
        return FindOrCreateOAuthResult::InternalError;
    }

    auto user = std::make_shared<DB::UserInfo>();
    user->setAccount(BuildOAuthAccount(db, input.open_id, input.login));
    user->setName(BuildOAuthName(db, input.open_id, input.name, input.login));
    user->setEmail(email);
    user->setAvatar(input.avatar);
    if (!input.login.empty()) {
        user->setGithub("https://github.com/" + input.login);
    }
    user->setPasswd(passwd_hash);
    user->setRole(UserRole::USER);
    user->setState(UserState::ACTIVE);
    int64_t now = chen::TimeUtil::GetCurrentSec();
    user->setCreateTime(now);
    user->setUpdateTime(now);

    if (!UserRepository::Insert(user, db)) {
        ERROR(logger) << "FindOrCreateOAuthUser insert failed, open_id=" << input.open_id << " errno=" << db->getErrno()
                      << " errstr=" << db->getErrStr();
        return FindOrCreateOAuthResult::DbError;
    }
    UserRepository::Put(user);
    out = user;
    return FindOrCreateOAuthResult::Ok;
}
