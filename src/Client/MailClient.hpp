/**
 * @file MailClient.hpp
 * @brief SMTP 邮件客户端
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include "ClientError.hpp"

#include <cstdint>
#include <string>

#include <Chen/Email/EMail.hpp>

class MailClient {
public:
    /**
     * @brief 发送邮件
     * @param host SMTP 服务器
     * @param port 端口
     * @param ssl 是否隐式 TLS
     * @param email 邮件内容与收发件人
     * @param timeout_ms 发送超时
     */
    static ClientError Send(const std::string& host,
        uint16_t port,
        bool ssl,
        const chen::EMail::ptr& email,
        int64_t timeout_ms = 5000);
};
