# Config — 业务配置访问层

业务侧只通过 `src/Config/` 暴露的访问函数读取配置，不再各自散落 `chen::Config::Lookup` 声明。
底层仍是框架的 `chen::Config`（YAML 加载 + 热加载 + `ConfigVar`），本目录只是一张集中登记表。

## 文件一览

| 文件                   | 覆盖的 key                                                                                                              |
|------------------------|-------------------------------------------------------------------------------------------------------------------------|
| `AppConfig.hpp/.cc`    | `redis.name`、`server.work_path`、`server.data_path`、`server.instance_id`、`mysql.dbs`、`cache.obj.enabled`、`admin.*` |
| `AuthConfig.hpp/.cc`   | `auth.*`、`webauthn.*`、`oauth.github.*`                                                                                |
| `SearchConfig.hpp/.cc` | `search.*`                                                                                                              |
| `AIConfig.hpp/.cc`     | `ai.*`                                                                                                                  |
| `RpcConfig.hpp/.cc`    | `rpc.*`（hub 地址、bus_id 身份、超时）                                                                                  |

## 使用方式

```cpp
#include "../Config/AppConfig.hpp"   // 按调用方所在目录使用相对路径

std::string dir = GetServerDataPath() + "/backups";
std::string redis_pool = GetRedisPoolName();
auto search = GetSearchConf();
auto rpc = GetRpcConf();          // hub_address / bus_id / 超时
if (search.enabled) { /* ... */ }
```

访问函数都按值返回：`ConfigVar::getValue()` 本身就是拷贝，热加载后下一次调用即拿到新值，
不要持有返回值引用。

## 新增配置项

1. 判断归属：能落到已有结构体（如 `SearchConf`）就加字段；否则新建访问函数。
2. 在对应 `.cc` 里用文件级 `static` 声明 `Config::Lookup`，在 `.hpp` 里加 `GetXxx()`。
3. 结构体类型需要 `operator==`，并为 `LexicalCast<std::string, T>` / `LexicalCast<T, std::string>` 提供特化。
   读字段统一用 `if (node["x"].IsScalar()) { ... }` 守卫：字符串直接取 `node["x"].Scalar()`，数值/布尔走
   `LexicalCast<std::string, T>()(node["x"].Scalar())`。不要用 `node.as<bool>()`（符号未导出），也不要依赖
   `as<std::string>(fallback)`——它对 YAML 空值会返回字面量 `"null"`。
4. 在 `bin/conf/*.yml.tpl` 与 env 文件里补上配置（见 `CONFIG.md`）。
5. 更新本文件的 key 表。

## 两条硬性约束

1. **必须在静态初始化阶段注册**：`chen::Library::GetModule()` 在 `dlopen` 之后会立刻
   `Config::LoadFromConfDir()`，晚于这个时点才 `Lookup` 的 key 拿不到 YAML 里的值（只会是默认值）。
   所以不要在函数内用 `static` 做懒注册。
2. **同一路径只能注册一个变量**：结构体变量（如 `search`）注册在父节点上，就不要再单独注册
   `search.timeout_ms` 之类的子节点，否则会重复赋值。

## 热加载

配置项本身没有注册 listener，文件变更后新值在下一次读取时生效。需要即时响应（重建连接池等）
的配置，应在使用方 `onServerUp`/`onServerReady` 里 `addListener`，并在 `onUnload` 里 `delListener`。
