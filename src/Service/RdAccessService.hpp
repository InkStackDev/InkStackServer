/**
 * @file RdAccessService.hpp
 * @brief 研发平台准入检查用例
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "../Domain/AccessTypes.hpp"

#include <cstdint>
#include <string>

class RdAccessService {
public:
    enum class Result {
        Ok,
        AccessDenied,
        DbError
    };

    struct CheckResult {
        bool can_access = false;
        std::string reason;
    };

    static Result CheckPlatformAccess(const Viewer& viewer, int64_t target_uid, CheckResult& out);
};
