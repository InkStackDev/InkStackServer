/**
 * @file ClientError.hpp
 * @brief 外部客户端统一错误与结果封装
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

enum class ClientError {
    Ok,
    Disabled,
    Timeout,
    ConnectFailed,
    Unauthorized,
    NotFound,
    RateLimited,
    ServerError,
    MalformedResponse,
    UnsupportedProvider,
    Unknown,
};

template <class T>
struct ClientResult {
    ClientError error = ClientError::Ok;
    T value{};
    std::string detail;

    bool ok() const { return error == ClientError::Ok; }
};
