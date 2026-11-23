/**
 * @file PasskeyService.hpp
 * @brief Passkey 凭据用例：注册、更新、删除
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "DB/UserPasskeyInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

class PasskeyService {
public:
    static DB::UserPasskeyInfo::ptr Get(int64_t id);

    static DB::UserPasskeyInfo::ptr GetByCredentialId(const std::string& credential_id);

    /// @return 有效凭据数量，DB 失败返回 -1
    static int ListByUser(std::vector<DB::UserPasskeyInfo::ptr>& results, int64_t uid);

    /// @return 0 成功，负数失败
    static int Add(DB::UserPasskeyInfo::ptr info);

    /// @return 0 成功，负数失败
    static int Update(DB::UserPasskeyInfo::ptr info);

    /// @return 0 成功，负数失败
    static int Remove(int64_t id);
};
