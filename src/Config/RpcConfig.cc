#include "RpcConfig.hpp"

// 配置项必须在模块 .so 的静态初始化阶段注册：chen::Library::GetModule() 在 dlopen 之后
// 会立刻调用 Config::LoadFromConfDir()，此时尚未注册的 key 不会被赋值。
static chen::ConfigVar<RpcConf>::ptr g_rpc_conf =
    chen::Config::Lookup("rpc", RpcConf(), "RPC hub client configuration");

RpcConf GetRpcConf()
{
    return g_rpc_conf->getValue();
}
