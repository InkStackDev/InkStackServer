/**
 * @file PasskeyRepository.hpp
 * @brief Passkey 凭据单表读写
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserPasskeyInfo.hpp"

#include <Chen/DB/DB.hpp>

#include <cstdint>
#include <string>
#include <vector>

class PasskeyRepository {
public:
    static DB::UserPasskeyInfo::ptr Get(int64_t id, chen::IDB::ptr conn);

    static DB::UserPasskeyInfo::ptr GetByCredentialId(const std::string& credential_id, chen::IDB::ptr conn);

    static bool ListByUser(std::vector<DB::UserPasskeyInfo::ptr>& results, int64_t uid, chen::IDB::ptr conn);

    static bool Insert(DB::UserPasskeyInfo::ptr info, chen::IDB::ptr conn);

    static bool Update(DB::UserPasskeyInfo::ptr info, chen::IDB::ptr conn);
};
