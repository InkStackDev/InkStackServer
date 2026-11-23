#include "MailClient.hpp"

#include <Chen/Email/Smtp.hpp>
#include <Chen/Log/Log.hpp>
#include <Chen/Util/TimeUtil.hpp>

static chen::Logger::ptr logger = LOG_ROOT();

ClientError MailClient::Send(const std::string& host,
    uint16_t port,
    bool ssl,
    const chen::EMail::ptr& email,
    int64_t timeout_ms)
{
    if (host.empty() || port == 0 || !email) {
        return ClientError::MalformedResponse;
    }

    const uint64_t connect_begin = chen::TimeUtil::GetCurrentMs();
    auto client = chen::SmtpClient::Create(host, port, ssl);
    const uint64_t connect_ms = chen::TimeUtil::GetCurrentMs() - connect_begin;
    if (!client) {
        ERROR(logger) << "connect smtp server fail: host=" << host << ":" << port << " ssl=" << (ssl ? 1 : 0)
                      << " cost=" << connect_ms << "ms";
        return ClientError::ConnectFailed;
    }

    const uint64_t send_begin = chen::TimeUtil::GetCurrentMs();
    auto result = client->send(email, timeout_ms);
    const uint64_t send_ms = chen::TimeUtil::GetCurrentMs() - send_begin;
    if (!result) {
        ERROR(logger) << "send email fail: smtp result is null, host=" << host << ":" << port
                      << " ssl=" << (ssl ? 1 : 0) << " cost=" << connect_ms + send_ms << "ms";
        return ClientError::ServerError;
    }
    if (result->result != 0) {
        ERROR(logger) << "send email fail: " << result->result << " " << result->msg << ", host=" << host << ":" << port
                      << " ssl=" << (ssl ? 1 : 0) << " cost=" << connect_ms + send_ms << "ms";
        return ClientError::ServerError;
    }

    INFO(logger) << "send email ok: host=" << host << ":" << port << " ssl=" << (ssl ? 1 : 0)
                 << " connect=" << connect_ms << "ms send=" << send_ms << "ms";
    return ClientError::Ok;
}
