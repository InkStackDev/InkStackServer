#include "UserService.hpp"

#include <memory>
#include <vector>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "DB/UserRecoveryCodeInfo.hpp"

#include "../Manager/UserManager.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/TotpUtil.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

DB::UserInfo::ptr UserService::Get(int64_t uid)
{
    auto db = GetDB();
    if (!db) {
        return nullptr;
    }
    return UserRepository::Get(uid, db);
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

    auto qb = DB::UserRecoveryCodeInfoDao::newQuery();
    if (!qb) {
        ERROR(logger) << "Disable2FA: create recovery code query failed, uid=" << uid;
        return Disable2faResult::DbError;
    }
    qb->where("user_id", "=", uid);
    if (qb->executeDelete(db)) {
        ERROR(logger) << "Disable2FA: delete recovery codes failed, uid=" << uid;
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
    info->setState(UserManager::Status::INACTIVE);
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
        info->setState(UserManager::Status::ACTIVE);
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
    revoke_sessions = !input.passwd.empty() || (input.has_state && input.state != UserManager::Status::ACTIVE);
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
