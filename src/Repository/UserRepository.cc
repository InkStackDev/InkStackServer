#include "UserRepository.hpp"

#include <memory>

#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

#include "../Cache/EntityCache.hpp"
#include "../Cache/CacheUtil.hpp"

static chen::Logger::ptr logger = LOG_ROOT();

struct UserRepositoryCodec {
    static DB::UserInfo::ptr Decode(const Json::Value& v)
    {
        auto info = std::make_shared<DB::UserInfo>();
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
};

const EntityCache<DB::UserInfo, UserRepositoryCodec> g_user_cache{{"usr", 120}};

DB::UserInfo::ptr UserRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return g_user_cache.Get(id, [id, conn]() {
        return DB::UserInfoDao::Query(id, conn);
    });
}

DB::UserInfo::ptr UserRepository::GetByAccount(const std::string& account, chen::IDB::ptr conn)
{
    if (!conn || account.empty()) {
        return nullptr;
    }
    auto info = DB::UserInfoDao::QueryByAccount(account, conn);
    if (info) {
        g_user_cache.Put(info);
    }
    return info;
}

DB::UserInfo::ptr UserRepository::GetByEmail(const std::string& email, chen::IDB::ptr conn)
{
    if (!conn || email.empty()) {
        return nullptr;
    }
    auto info = DB::UserInfoDao::QueryByEmail(email, conn);
    if (info) {
        g_user_cache.Put(info);
    }
    return info;
}

DB::UserInfo::ptr UserRepository::GetByName(const std::string& name, chen::IDB::ptr conn)
{
    if (!conn || name.empty()) {
        return nullptr;
    }
    auto info = DB::UserInfoDao::QueryByName(name, conn);
    if (info) {
        g_user_cache.Put(info);
    }
    return info;
}

bool UserRepository::Insert(DB::UserInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserInfoDao::Insert(info, conn) == 0;
}

bool UserRepository::Update(DB::UserInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserInfoDao::Update(info, conn) == 0;
}

bool UserRepository::InsertOrUpdate(DB::UserInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserInfoDao::InsertOrUpdate(info, conn) == 0;
}

void UserRepository::GetAllIds(std::vector<int64_t>& ids, bool is_valid, chen::IDB::ptr conn)
{
    if (!conn) {
        ERROR(logger) << "GetAllIds: null connection";
        return;
    }
    auto qb = DB::UserInfoDao::newQuery();
    if (!qb) {
        ERROR(logger) << "GetAllIds: create query failed";
        return;
    }
    qb->select("id");
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->queryColumn<int64_t>(ids, conn, "id");
}

uint64_t UserRepository::ListByPages(std::vector<DB::UserInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t role,
    int32_t state,
    int32_t days,
    bool is_valid,
    chen::IDB::ptr conn)
{
    if (!conn) {
        ERROR(logger) << "ListByPages: null connection";
        return 0;
    }

    int64_t start_time = 0;
    if (days > 0) {
        start_time = chen::TimeUtil::GetCurrentSec() - days * 24 * 3600;
    }

    auto qb = DB::UserInfoDao::newQuery();
    if (!qb) {
        ERROR(logger) << "ListByPages: create query failed";
        return 0;
    }
    qb->whereIf(role != -1, "role", "=", (int64_t)role);
    qb->whereIf(state != -1, "state", "=", (int64_t)state);
    qb->whereIf(days > 0, "create_time", ">=", start_time);
    qb->whereIf(is_valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (DB::UserInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, conn)) {
        return 0;
    }
    return total > 0 ? static_cast<uint64_t>(total) : 0;
}

void UserRepository::Put(const DB::UserInfo::ptr& info)
{
    g_user_cache.Put(info);
}

void UserRepository::Evict(int64_t id)
{
    g_user_cache.Evict(id);
}
