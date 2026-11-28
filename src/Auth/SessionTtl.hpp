/**
 * @file SessionTtl.hpp
 * @brief 会话有效期：读取系统设置，提供 session TTL
 * @author Christins
 * @date 2026-06-30
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>

/**
 * @brief 获取 session TTL（秒），优先读取系统设置，默认 24 小时
 */
int64_t GetSessionTtlSeconds();
