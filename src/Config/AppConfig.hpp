/**
 * @file AppConfig.hpp
 * @brief 业务侧基础设施配置访问入口（Redis / 服务路径 / MySQL / 对象缓存 / 引导管理员）
 * @author Christins
 * @date 2026-11-22
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Config/Config.hpp>

#include <map>
#include <sstream>
#include <string>

/**
 * @brief 引导超级管理员配置
 * @details 仅当库中不存在对应邮箱的账号时才用于创建管理员，已存在的账号不会被覆盖
 */
struct AdminConf {
    std::string account = "admin";
    std::string passwd;
    std::string email = "admin@example.com";

    bool operator==(const AdminConf& other) const
    {
        return account == other.account && passwd == other.passwd && email == other.email;
    }
};

namespace chen {

template <>
class LexicalCast<std::string, AdminConf> {
public:
    AdminConf operator()(const std::string& value)
    {
        AdminConf conf;
        const YAML::Node node = YAML::Load(value);
        if (!node.IsMap()) {
            return conf;
        }
        if (node["account"].IsScalar()) {
            conf.account = node["account"].Scalar();
        }
        if (node["passwd"].IsScalar()) {
            conf.passwd = node["passwd"].Scalar();
        }
        if (node["email"].IsScalar()) {
            conf.email = node["email"].Scalar();
        }
        return conf;
    }
};

template <>
class LexicalCast<AdminConf, std::string> {
public:
    std::string operator()(const AdminConf& conf)
    {
        YAML::Node node;
        node["account"] = conf.account;
        node["passwd"] = conf.passwd;
        node["email"] = conf.email;
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

} // namespace chen

/**
 * @brief Redis 连接池名称
 */
std::string GetRedisPoolName();

/**
 * @brief 部署目录（server.work_path）
 */
std::string GetServerWorkPath();

/**
 * @brief 数据目录（server.data_path，备份/导出文件根目录）
 */
std::string GetServerDataPath();

/**
 * @brief MySQL 连接配置（mysql.dbs）
 */
std::map<std::string, std::map<std::string, std::string>> GetMysqlDbs();

/**
 * @brief 是否启用 Redis 对象缓存（cache.obj.enabled）
 */
bool GetObjCacheEnabled();

/**
 * @brief 引导超级管理员配置（admin）
 */
AdminConf GetAdminConf();
