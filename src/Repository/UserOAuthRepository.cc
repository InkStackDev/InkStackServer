#include "UserOAuthRepository.hpp"

DB::UserOauthInfo::ptr UserOAuthRepository::GetByOpenId(const std::string& provider,
    const std::string& open_id,
    chen::IDB::ptr conn)
{
    if (!conn || provider.empty() || open_id.empty()) {
        return nullptr;
    }
    auto qb = DB::UserOauthInfoDao::newQuery();
    if (!qb) {
        return nullptr;
    }
    qb->where("provider", "=", provider);
    qb->where("open_id", "=", open_id);
    std::vector<DB::UserOauthInfo::ptr> results;
    if (DB::UserOauthInfoDao::QueryByBuilder(results, qb, conn) || results.empty()) {
        return nullptr;
    }
    return results[0];
}

DB::UserOauthInfo::ptr UserOAuthRepository::GetByUser(int64_t user_id, const std::string& provider, chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0 || provider.empty()) {
        return nullptr;
    }
    auto qb = DB::UserOauthInfoDao::newQuery();
    if (!qb) {
        return nullptr;
    }
    qb->where("user_id", "=", user_id);
    qb->where("provider", "=", provider);
    std::vector<DB::UserOauthInfo::ptr> results;
    if (DB::UserOauthInfoDao::QueryByBuilder(results, qb, conn) || results.empty()) {
        return nullptr;
    }
    return results[0];
}

bool UserOAuthRepository::Insert(DB::UserOauthInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserOauthInfoDao::Insert(info, conn) == 0;
}
