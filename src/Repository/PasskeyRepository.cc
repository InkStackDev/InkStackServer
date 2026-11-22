#include "PasskeyRepository.hpp"

DB::UserPasskeyInfo::ptr PasskeyRepository::Get(int64_t id, chen::IDB::ptr conn)
{
    if (!conn || id <= 0) {
        return nullptr;
    }
    return DB::UserPasskeyInfoDao::Query(id, conn);
}

DB::UserPasskeyInfo::ptr PasskeyRepository::GetByCredentialId(const std::string& credential_id, chen::IDB::ptr conn)
{
    if (!conn || credential_id.empty()) {
        return nullptr;
    }
    return DB::UserPasskeyInfoDao::QueryByCredentialId(credential_id, conn);
}

bool PasskeyRepository::ListByUser(std::vector<DB::UserPasskeyInfo::ptr>& results, int64_t uid, chen::IDB::ptr conn)
{
    if (!conn || uid <= 0) {
        return false;
    }
    return DB::UserPasskeyInfoDao::QueryByUserId(results, uid, conn) == 0;
}

bool PasskeyRepository::Insert(DB::UserPasskeyInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserPasskeyInfoDao::Insert(info, conn) == 0;
}

bool PasskeyRepository::Update(DB::UserPasskeyInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserPasskeyInfoDao::Update(info, conn) == 0;
}
