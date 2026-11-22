/**
 * @file UserRecoveryCodeRepository.hpp
 * @brief 2FA 恢复码单表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserRecoveryCodeInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>

class UserRecoveryCodeRepository {
public:
    static DB::UserRecoveryCodeInfo::ptr FindUnused(int64_t user_id, const std::string& code_hash, chen::IDB::ptr conn);

    static bool DeleteByUser(int64_t user_id, chen::IDB::ptr conn);

    static bool Insert(DB::UserRecoveryCodeInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::UserRecoveryCodeInfo::ptr info, chen::IDB::ptr conn);
};
