#include "AppConfig.hpp"

// 配置项必须在模块 .so 的静态初始化阶段注册：chen::Library::GetModule() 在 dlopen 之后
// 会立刻调用 Config::LoadFromConfDir()，此时尚未注册的 key 不会被赋值。
static chen::ConfigVar<std::string>::ptr g_redis_name =
    chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

static chen::ConfigVar<std::string>::ptr g_server_work_path =
    chen::Config::Lookup("server.work_path", std::string(""), "server work path");

static chen::ConfigVar<std::string>::ptr g_server_data_path =
    chen::Config::Lookup("server.data_path", std::string(""), "server data storage path");

static chen::ConfigVar<int32_t>::ptr g_server_instance_id =
    chen::Config::Lookup("server.instance_id", int32_t(1), "server instance id");

static chen::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
    chen::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

static chen::ConfigVar<bool>::ptr g_obj_cache_enabled =
    chen::Config::Lookup("cache.obj.enabled", true, "enable redis object cache");

static chen::ConfigVar<AdminConf>::ptr g_admin_conf =
    chen::Config::Lookup("admin", AdminConf(), "bootstrap super admin");

std::string GetRedisPoolName()
{
    return g_redis_name->getValue();
}

std::string GetServerWorkPath()
{
    return g_server_work_path->getValue();
}

std::string GetServerDataPath()
{
    return g_server_data_path->getValue();
}

int32_t GetServerInstanceId()
{
    return g_server_instance_id->getValue();
}

std::map<std::string, std::map<std::string, std::string>> GetMysqlDbs()
{
    return g_mysql_dbs->getValue();
}

bool GetObjCacheEnabled()
{
    return g_obj_cache_enabled->getValue();
}

AdminConf GetAdminConf()
{
    return g_admin_conf->getValue();
}
