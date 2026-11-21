#include "UserManager.hpp"

#include "../Util/Util.hpp"
#include "CacheUtil.hpp"

#include <Chen/Config/Config.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/JsonUtil.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 120;
static const char* kCachePrefix = "cache:obj:usr:";

static std::string UserCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::UserInfo::ptr ParseUserCache(const Json::Value& v)
{
    auto info = std::make_shared<data::UserInfo>();
    info->setId(CacheJsonInt64(v, "id"));
    info->setName(CacheJsonString(v, "name"));
    info->setAccount(CacheJsonString(v, "account"));
    info->setAvatar(CacheJsonString(v, "avatar"));
    info->setEmail(CacheJsonString(v, "email"));
    info->setRole(CacheJsonInt32(v, "role"));
    info->setPasswd(CacheJsonString(v, "passwd"));
    info->setState(CacheJsonInt32(v, "state"));
    info->setBio(CacheJsonString(v, "bio"));
    info->setWebsite(CacheJsonString(v, "website"));
    info->setGithub(CacheJsonString(v, "github"));
    info->setLocation(CacheJsonString(v, "location"));
    info->setToken(CacheJsonString(v, "token"));
    info->setTokenTime(CacheJsonInt64(v, "token_time"));
    info->setLoginTime(CacheJsonTime(v, "login_time"));
    info->setIsDeleted(CacheJsonInt32(v, "is_deleted"));
    info->setCreateTime(CacheJsonTime(v, "create_time"));
    info->setUpdateTime(CacheJsonTime(v, "update_time"));
    info->setTotpSecret(CacheJsonString(v, "totp_secret"));
    info->setTotpEnabled(CacheJsonInt32(v, "totp_enabled"));
    info->markClean();
    return info;
}

static data::UserInfo::ptr GetUserCache(int64_t id)
{
    std::string key = UserCacheKey(id);
    std::string data = RedisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        RedisCacheDel(key);
        return nullptr;
    }
    return ParseUserCache(v);
}

static void SetUserCache(const data::UserInfo::ptr& info)
{
    if (!info) {
        return;
    }
    RedisCacheSetex(UserCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

static chen::ConfigVar<std::string>::ptr g_admin_account = chen::Config::Lookup("admin.account", std::string("admin"), "default super admin account");
static chen::ConfigVar<std::string>::ptr g_admin_passwd =
    chen::Config::Lookup("admin.passwd", std::string(""), "bootstrap super admin password (required only on first seed)");
static chen::ConfigVar<std::string>::ptr g_admin_email =
    chen::Config::Lookup("admin.email", std::string("admin@example.com"), "default super admin email");

data::UserInfo::ptr UserManager::ParseRow(chen::ISQLData::ptr rt)
{
    return data::UserInfoDao::ParseRow(rt);
}

void UserManager::add(blog::data::UserInfo::ptr info)
{
    SetUserCache(info);
}

void UserManager::update(blog::data::UserInfo::ptr info)
{
    SetUserCache(info);
}

void UserManager::getAllIds(std::vector<int64_t>& ids, bool is_valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::UserInfoDao::newQuery();
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->select("id");
    qb->queryColumn<int64_t>(ids, db, "id");
}

uint64_t UserManager::listByPages(
    std::vector<blog::data::UserInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t role,
    int32_t state,
    int32_t days,
    bool is_valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    int64_t start_time = 0;
    if (days > 0) {
        start_time = time(0) - days * 24 * 3600;
    }

    auto qb = data::UserInfoDao::newQuery();
    qb->whereIf(role != -1, "role", "=", (int64_t)role);
    qb->whereIf(state != -1, "state", "=", (int64_t)state);
    qb->whereIf(days > 0, "create_time", ">=", start_time);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::UserInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    return total;
}

blog::data::UserInfo::ptr UserManager::get(int64_t id)
{
    auto v = GetUserCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::UserInfoDao::Query(id, db);
    if (v) {
        SetUserCache(v);
    }
    return v;
}

blog::data::UserInfo::ptr UserManager::getByAccount(const std::string& v)
{
    int64_t cachedId = GetCachedIdMapping("usr:acct:" + v);
    if (cachedId > 0) {
        return get(cachedId);
    }
    // Check cache first
    // Since cache is only keyed by id, query DB directly via DAO
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::UserInfoDao::QueryByAccount(v, db);
    if (info) {
        SetUserCache(info);
        CacheIdMapping("usr:acct:" + v, info->getId());
    }
    return info;
}

blog::data::UserInfo::ptr UserManager::getByEmail(const std::string& v)
{
    int64_t cachedId = GetCachedIdMapping("usr:eml:" + v);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::UserInfoDao::QueryByEmail(v, db);
    if (info) {
        SetUserCache(info);
        CacheIdMapping("usr:eml:" + v, info->getId());
    }
    return info;
}

void UserManager::ensureSuperAdmin()
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    std::string account = g_admin_account->getValue();
    std::string passwd = g_admin_passwd->getValue();
    std::string email = g_admin_email->getValue();
    if (email.empty()) {
        ERROR(logger) << "admin.email empty, skip seeding super admin";
        return;
    }

    auto info = data::UserInfoDao::QueryByEmail(email, db);
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
        info.reset(new data::UserInfo);
        info->setPasswd(passwd_hash);
        info->setName(account);
        info->setAccount(account);
        info->setEmail(email);
        is_new = true;
    }

    info->setRole(Role::ADMIN);
    info->setState(Status::ACTIVE);
    info->setIsDeleted(0);

    if (data::UserInfoDao::InsertOrUpdate(info, db)) {
        ERROR(logger) << "insert super admin failed: errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return;
    }
    SetUserCache(info);

    if (is_new) {
        INFO(logger) << "seeded super admin account=" << account << " email=" << email;
    } else {
        INFO(logger) << "super admin ensured account=" << info->getAccount() << " email=" << email;
    }
}

blog::data::UserInfo::ptr UserManager::getByName(const std::string& v)
{
    int64_t cachedId = GetCachedIdMapping("usr:name:" + v);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::UserInfoDao::QueryByName(v, db);
    if (info) {
        SetUserCache(info);
        CacheIdMapping("usr:name:" + v, info->getId());
    }
    return info;
}

std::string UserManager::GetToken(data::UserInfo::ptr info, int64_t us)
{
    std::stringstream ss;
    ss << info->getId() << "|" << info->getAccount() << "|" << info->getEmail() << "|" << info->getPasswd() << "|" << us;
    return chen::EncryptorUtil::MD5(ss.str());
}

std::string UserManager::GenerateToken()
{
    std::stringstream ss;
    ss << std::hex << chen::TimeUtil::GetCurrentUs() << rand() << rand();
    return chen::EncryptorUtil::MD5(ss.str());
}
