#include "AIConfig.hpp"

// 配置项必须在模块 .so 的静态初始化阶段注册：chen::Library::GetModule() 在 dlopen 之后
// 会立刻调用 Config::LoadFromConfDir()，此时尚未注册的 key 不会被赋值。
static chen::ConfigVar<AIConf>::ptr g_ai_conf = chen::Config::Lookup("ai", AIConf(), "system default AI configuration");

AIConf GetAIConf()
{
    return g_ai_conf->getValue();
}
