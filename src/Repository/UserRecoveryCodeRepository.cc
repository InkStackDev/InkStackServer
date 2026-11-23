#include "UserRecoveryCodeRepository.hpp"

DB::UserRecoveryCodeInfo::ptr UserRecoveryCodeRepository::FindUnused(int64_t user_id,
    const std::string& code_hash,
    chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0 || code_hash.empty()) {
        return nullptr;
    }
    auto qb = DB::UserRecoveryCodeInfoDao::newQuery();
    if (!qb) {
        return nullptr;
    }
    qb->where("user_id", "=", user_id);
    qb->where("code_hash", "=", code_hash);
    qb->where("used", "=", (int64_t)0);

    std::vector<DB::UserRecoveryCodeInfo::ptr> codes;
    if (DB::UserRecoveryCodeInfoDao::QueryByBuilder(codes, qb, conn) || codes.empty()) {
        return nullptr;
    }
    return codes[0];
}

bool UserRecoveryCodeRepository::DeleteByUser(int64_t user_id, chen::IDB::ptr conn)
{
    if (!conn || user_id <= 0) {
        return false;
    }
    auto qb = DB::UserRecoveryCodeInfoDao::newQuery();
    if (!qb) {
        return false;
    }
    qb->where("user_id", "=", user_id);
    return qb->executeDelete(conn) == 0;
}

bool UserRecoveryCodeRepository::Insert(DB::UserRecoveryCodeInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserRecoveryCodeInfoDao::Insert(info, conn) == 0;
}

bool UserRecoveryCodeRepository::Update(DB::UserRecoveryCodeInfo::ptr info, chen::IDB::ptr conn)
{
    if (!info || !conn) {
        return false;
    }
    return DB::UserRecoveryCodeInfoDao::Update(info, conn) == 0;
}
