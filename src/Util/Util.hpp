/**
 * @file Util.hpp
 * @brief 通用工具函数（邮箱/账号校验、用户ID编解码等）
 * @author Christins
 * @date 2026-06-03
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/DB/MySQL.hpp>
#include <Chen/Http/SSESession.hpp>
#include <Chen/Util/Util.hpp>

#include <regex>
#include <sys/statvfs.h>

chen::IDB::ptr GetDB();

/**
 * @brief 生成密码哈希（bcrypt，含随机盐）
 * @param password 明文密码
 * @return bcrypt 哈希；失败返回空串
 */
std::string HashPassword(const std::string& password);

/**
 * @brief 校验密码，兼容历史无盐 MD5 哈希
 * @param password 明文密码
 * @param stored 数据库中保存的哈希
 * @return 是否匹配
 */
bool VerifyPassword(const std::string& password, const std::string& stored);

/**
 * @brief 判断是否为历史 MD5 哈希（登录成功后用于透明升级为 bcrypt）
 * @param stored 数据库中保存的哈希
 * @return 是否为 32 位十六进制 MD5
 */
bool IsLegacyPasswordHash(const std::string& stored);

struct DiskUsage {
    int64_t total_size;
    int64_t used_size;
    int64_t available_size;
};

/**
 * @brief 当前进程内存与线程信息（读取 /proc/self/status）
 * @note 非 Linux 平台或读取失败时字段为 -1
 */
struct ProcessMemoryInfo {
    int64_t rss_kb = -1;
    int64_t vm_kb = -1;
    int64_t threads = -1;
};

/**
 * @brief 系统内存信息（读取 /proc/meminfo）
 * @note 非 Linux 平台或读取失败时字段为 -1
 */
struct SystemMemoryInfo {
    int64_t total_kb = -1;
    int64_t available_kb = -1;
};

/**
 * @brief 读取当前进程内存与线程信息
 */
ProcessMemoryInfo GetProcessMemoryInfo();

/**
 * @brief 读取系统内存信息
 */
SystemMemoryInfo GetSystemMemoryInfo();

inline DiskUsage GetDiskUsage(const std::string& path = ".")
{
    DiskUsage du = {0, 0, 0};
    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) == 0) {
        du.total_size = vfs.f_frsize * vfs.f_blocks;
        du.available_size = vfs.f_frsize * vfs.f_bavail;
        du.used_size = du.total_size - du.available_size;
    }
    return du;
}

inline bool IsEmail(const std::string& str)
{
    static const std::regex pattern("([0-9A-Za-z\\-_\\.]+)@([0-9a-z]+\\.[a-z]{2,8}(\\.[a-z]{2,8})?)");
    return std::regex_match(str, pattern);
}

/**
 * @brief 解析 SMTP 加密方式，判断是否需要隐式 TLS（SMTPS）
 * @param encryption 系统设置中的 smtp_encryption：none / ssl / tls
 * @param use_ssl 输出：true 表示连接后立即 TLS 握手（465），false 表示明文连接
 * @return 当前 SDK 是否支持该加密方式
 * @note chen::SmtpClient 只实现了隐式 TLS，不实现 STARTTLS（587），
 *       因此 tls/starttls 配置会被判为不支持，避免明文连接被服务端拒绝。
 */
inline bool ResolveSmtpTls(const std::string& encryption, bool& use_ssl)
{
    std::string mode = chen::StringUtil::ToLower(encryption);
    if (mode.empty() || mode == "ssl" || mode == "smtps") {
        // 未配置时保持历史行为：按隐式 TLS 处理
        use_ssl = true;
        return true;
    }
    if (mode == "none" || mode == "plain" || mode == "off") {
        use_ssl = false;
        return true;
    }
    // tls / starttls：需要在明文连接上升级，SDK 不支持
    use_ssl = false;
    return false;
}

inline bool IsValidAccount(const std::string& str)
{
    static const std::regex s_account_regex("[A-Za-z][0-9A-Za-z\\-_\\.]{4,15}");
    return std::regex_match(str, s_account_regex);
}

static const std::string kApiKeySecret = "BlogServer!2025$%^APIKey#@!SecretKey";

inline std::string EncryptApiKey(const std::string& api_key)
{
    if (api_key.empty()) {
        return "";
    }
    std::string data = api_key;
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= kApiKeySecret[i % kApiKeySecret.size()];
    }
    return chen::StringUtil::Base64Encode(data);
}

inline std::string DecryptApiKey(const std::string& encrypted)
{
    if (encrypted.empty()) {
        return "";
    }
    std::string data = chen::StringUtil::Base64Decode(encrypted);
    if (data.empty()) {
        return "";
    }
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= kApiKeySecret[i % kApiKeySecret.size()];
    }
    return data;
}

inline std::string MaskApiKey(const std::string& api_key)
{
    if (api_key.length() <= 8) {
        return std::string(api_key.length(), '*');
    }
    // 保留前4位和后4位，中间用星号替代
    return api_key.substr(0, 3) + std::string(api_key.length() - 8, '*') + api_key.substr(api_key.length() - 2);
}

inline void SendWX(const std::string& group, const std::string& msg)
{
    // TODO: ...
}

#define DEFINE_AND_CHECK_STRING(result, var, param)                                 \
    std::string var = request->getParam(param);                                     \
    if (var.empty()) {                                                                                                 \
        result->setErrnoFmt(PARAM_MISSING, "param %s is required", param);                                             \
        break;                                                                      \
    }

#define DEFINE_AND_CHECK_STRING_WITH_DEFAULT(result, var, param, default_val) std::string var = request->getParam(param, default_val);

#define DEFINE_AND_CHECK_TYPE(result, type, var, param)                             \
    type var;                                                                       \
    if (!request->checkGetParamAs(param, var)) {                                                                       \
        result->setErrnoFmt(PARAM_MISSING, "param %s is required", param);                                             \
        break;                                                                      \
    }

inline void SendSSEJson(chen::http::SSESession::ptr session, const std::string& type, const std::string& key, const std::string& value)
{
    Json::Value obj;
    obj["type"] = type;
    obj[key] = value;
    session->sendEvent(chen::JsonUtil::ToString(obj));
}

std::string GetRemoteIP(chen::http::HttpRequest::ptr request, chen::http::HttpSession::ptr session);
