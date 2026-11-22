#include "UserService.hpp"

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "DB/UserRecoveryCodeInfo.hpp"

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
