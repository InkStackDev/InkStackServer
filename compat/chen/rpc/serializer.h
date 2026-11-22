/**
 * @file serializer.h
 * @brief 兼容旧协议生成器输出的 RPC 序列化头路径
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

// 旧版 protocol 生成器仍输出 <chen/rpc/serializer.h>，
// 当前 SDK 的实现在 <Chen/Rpc/Serializer.hpp>。
#include <Chen/Rpc/Serializer.hpp>
