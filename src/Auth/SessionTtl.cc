#include "SessionTtl.hpp"

#include "../Cache/SystemSettingsStore.hpp"

/// 默认 session TTL（24 小时 = 86400 秒）
static const int64_t kDefaultSessionTtl = 24 * 3600;

int64_t GetSessionTtlSeconds()
{
    auto settings = SystemSettingsStoreRegistry::GetInstance()->get();
    if (settings && settings->getSessionTimeout() > 0) {
        return static_cast<int64_t>(settings->getSessionTimeout()) * 3600;
    }
    return kDefaultSessionTtl;
}
