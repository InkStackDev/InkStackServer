#include "UserManager.hpp"

#include <sstream>

#include <Chen/Config/Config.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/EncryptorUtil.hpp>
#include <Chen/Util/RandomUtil.hpp>

#include "../Config/AppConfig.hpp"
#include "../Repository/UserRepository.hpp"
#include "../Util/Util.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

void UserManager::add(DB::UserInfo::ptr info)
{
    UserRepository::Put(info);
}

void UserManager::update(DB::UserInfo::ptr info)
{
    UserRepository::Put(info);
}

void UserManager::getAllIds(std::vector<int64_t>& ids, bool is_valid)
{
    UserRepository::GetAllIds(ids, is_valid, GetDB());
}

uint64_t UserManager::listByPages(std::vector<DB::UserInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t role,
    int32_t state,
    int32_t days,
    bool is_valid)
{
    return UserRepository::ListByPages(infos, offset, size, role, state, days, is_valid, GetDB());
}

DB::UserInfo::ptr UserManager::get(int64_t id)
{
    return UserRepository::Get(id, GetDB());
}

DB::UserInfo::ptr UserManager::getByAccount(const std::string& v)
{
    return UserRepository::GetByAccount(v, GetDB());
}

DB::UserInfo::ptr UserManager::getByEmail(const std::string& v)
{
    return UserRepository::GetByEmail(v, GetDB());
}

DB::UserInfo::ptr UserManager::getByName(const std::string& v)
{
    return UserRepository::GetByName(v, GetDB());
}

void UserManager::ensureSuperAdmin()
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    const AdminConf admin = GetAdminConf();
    std::string account = admin.account;
    std::string passwd = admin.passwd;
    std::string email = admin.email;
    if (email.empty()) {
        ERROR(logger) << "admin.email empty, skip seeding super admin";
        return;
    }

    auto info = UserRepository::GetByEmail(email, db);
    bool is_new = false;
    if (!info) {
        // 仅在首次创建时要求配置初始密码，已有管理员不会被重置密码
        if (account.empty() || passwd.empty()) {
            ERROR(logger) << "super admin not found and admin.account/admin.passwd empty, "
                          << "please configure them to bootstrap the first admin";
            return;
        }
        std::string passwd_hash = HashPassword(passwd);
        if (passwd_hash.empty()) {
            ERROR(logger) << "hash super admin password failed, skip seeding";
            return;
        }
        info.reset(new DB::UserInfo);
        info->setPasswd(passwd_hash);
        info->setName(account);
        info->setAccount(account);
        info->setEmail(email);
        is_new = true;
    }

    info->setRole(Role::ADMIN);
    info->setState(Status::ACTIVE);
    info->setIsDeleted(0);

    if (!UserRepository::InsertOrUpdate(info, db)) {
        ERROR(logger) << "insert super admin failed: errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return;
    }
    UserRepository::Put(info);

    if (is_new) {
        INFO(logger) << "seeded super admin account=" << account << " email=" << email;
    } else {
        INFO(logger) << "super admin ensured account=" << info->getAccount() << " email=" << email;
    }
}

std::string UserManager::GetToken(DB::UserInfo::ptr info, int64_t us)
{
    if (!info) {
        return {};
    }
    std::stringstream ss;
    ss << info->getId() << "|" << info->getAccount() << "|" << info->getEmail() << "|" << info->getPasswd() << "|" << us;
    return chen::EncryptorUtil::MD5(ss.str());
}

std::string UserManager::GenerateToken()
{
    return chen::EncryptorUtil::MD5(chen::RandomUtil::RandString(32));
}
