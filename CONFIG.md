# 配置管理指南

本项目的运行配置位于 `bin/conf/`，通过模板 + 环境变量 + 渲染脚本的方式管理，支持在开发与生产环境之间快速切换。

## 1. 文件一览

| 文件 | 说明 |
|------|------|
| `bin/conf/system.yml.tpl` | 运行时配置模板：服务端口、日志、MySQL、Redis、工作线程 |
| `bin/conf/TechHaven.yml.tpl` | 业务配置模板：AI、超级管理员、分词字典/索引路径 |
| `bin/conf/system.yml` | 由模板渲染出的**实际生效**配置（被框架加载，**不入库**） |
| `bin/conf/TechHaven.yml` | 由模板渲染出的**实际生效**配置（被框架加载，**不入库**） |
| `bin/conf/dev.env` | 开发环境变量（入库） |
| `bin/conf/prod.env.example` | 生产环境变量模板（入库） |
| `bin/conf/prod.env` | 生产环境变量（**不入库**，由运维从 example 复制后填写真实值） |
| `switch-env.sh` | 切换/渲染脚本 |

> `bin/conf/` 下只有 `.yml` 会被框架加载；`.tpl` / `.env` / `.md` 不会被加载。

> **超级管理员引导**：库中不存在 `admin.email` / `admin.account` 对应账号时才会创建管理员，初始密码取
> `<ENV>.env` 中的 `ADMIN_PASSWD`（无默认密码；留空则跳过引导）。已存在的管理员不会被覆盖。

## 2. 切换环境

```sh
# 切换为开发配置
./switch-env.sh dev

# 切换为生产配置（首次需先准备 prod.env）
cp bin/conf/prod.env.example bin/conf/prod.env   # 填写真实值后
./switch-env.sh prod
```

脚本行为：

- 根据仓库根目录自动推导 `WORK_PATH`，无需按机器手动修改绝对路径。
- 优先使用 `envsubst` 渲染，未安装时自动回退到 `sed`。
- 渲染后校验是否残留未替换的 `${...}`，存在则报错并提示缺哪个变量。
- **只切换配置，不重启服务**。运行中的 worker 通过 inotify 监听 `conf/`，文件变更后会重新加载配置并触发 `ConfigVar`
  listener；如需同时替换模块 `.so`，发送 `SIGHUP` 让 master fork 新 worker（旧 worker drain 后退出）。

> **接入端口**：HTTP / WebSocket 端口由 `HTTP_PORT` / `WS_PORT` 控制（默认 8090 / 8091）。业务只作为 RPC Hub
> 客户端，不再监听 rpc 端口，配置里也没有 `type: rpc` 的 server 项。

> **多实例部署**：`make cluster ENV=prod INSTANCES=3` 生成 `techhaven-cluster-*.tar.gz`，包内
> `server_1|server_2|server_3/` 的配置已渲染好（端口、`instance_id`、日志与 pid 彼此独立），部署端直接
> `scripts/clusterctl.sh start all` 即可；细节见 [`docs/multi-instance.md`](docs/multi-instance.md)。

## 3. 新增一个配置项

先判断这个配置项属于哪一类：

- **各环境值不同**（如路径、数据库、日志级别）→ 走占位符 + env 文件。
- **值恒定不变**（如 `admin.account`）→ 直接写死在模板里，不进 env。

下面以新增「上传目录 `upload.dir`」为例，说明完整流程。

### 3.1 在模板中加占位符

按配置所属类别选模板：运行时配置进 `system.yml.tpl`，业务配置进 `TechHaven.yml.tpl`。

```yaml
# bin/conf/TechHaven.yml.tpl
upload:
  dir: ${UPLOAD_DIR}
```

### 3.2 在两个 env 文件里加值

```ini
# bin/conf/dev.env
UPLOAD_DIR=uploads

# bin/conf/prod.env.example（prod.env 同步）
UPLOAD_DIR=/data/techhaven/uploads
```

### 3.3 重新渲染

```sh
./switch-env.sh dev   # 或 prod
```

渲染后 `bin/conf/TechHaven.yml` 里会出现对应的真实值。

## 4. 在 C++ 代码中读取

业务代码统一通过 `src/Config/` 提供的方法读取配置，不要在业务文件里各自写 `chen::Config::Lookup`：

```cpp
#include "../Config/AppConfig.hpp"

std::string dir = GetServerWorkPath() + "/uploads";
std::string pool = GetRedisPoolName();
auto search = GetSearchConf();
```

可用方法与 key 对照表见 [`src/Config/README.md`](src/Config/README.md)。

确实需要新增配置项时，在 `src/Config/` 对应文件里声明：

```cpp
// .cc：必须放在文件级 static 上，在模块 dlopen 时完成注册
static chen::ConfigVar<std::string>::ptr g_upload_dir =
    chen::Config::Lookup("upload.dir", std::string("uploads"), "上传目录");

// .hpp
std::string GetUploadDir();
```

key 用 `.` 对应 YAML 层级（`upload.dir` ↔ `upload: dir:`），并提供默认值。 **不要在函数内用 `static` 做懒注册**：框架在
`dlopen` 之后会立刻重新加载配置，
晚于这个时点注册的 key 只会拿到默认值。

### 4.1 支持的配置类型

| C++ 类型 | 说明 |
|----------|------|
| `std::string` | 字符串 |
| `bool` | 布尔（YAML 的 true/false/1/0） |
| `int32_t` / `int64_t` / `double` 等 | 数值（经 `boost::lexical_cast` 转换） |
| `std::vector<T>` | 列表 |
| `std::list<T>` | 列表 |
| `std::set<T>` / `std::unordered_set<T>` | 集合 |
| `std::map<std::string, T>` | 键值表（对应 YAML 嵌套映射） |

> 参考现有用法：`src/Config/AppConfig.cc` 中的 `mysql.dbs`、`src/Config/SearchConfig.cc` 中的 `search` 结构体、
> `src/Config/AppConfig.cc` 中的 `admin` 结构体。

### 4.2 配置变更监听（热重载）

运行中的 worker 通过 inotify 监听 `conf/` 目录；配置文件变化时会调用
`Config::LoadFromFile` 重新加载，值发生变化的 `ConfigVar` 会触发已注册的 listener：

```cpp
uint64_t id = g_upload_dir->addListener([](const std::string& old_val, const std::string& new_val) {
    // 处理配置变更
});
```

注意：

- listener 在 **每个 worker 进程内**各注册、各回调一次；master 不加载业务模块，也不注册业务 listener。
- 模块退出前用 `delListener(id)` 注销；多进程模型下旧 worker 退出不会影响新 worker，但保持清理习惯。
- 端口、监听地址、`server.work_path` 等启动期配置由 master 在启动时使用，修改后需要冷重启；`SIGHUP` 只会 fork 新 worker，不会让
  master 重新绑定端口。
- `SIGHUP` 的语义是“换 worker”：master fork 新 worker，重新加载配置和磁盘上的 `.so`；旧 worker drain 后退出，不是在当前
  worker 内原地重载。

## 5. Redis 对象缓存

高频按 id 查询的实体（用户、文章、评论、通知、组织等）使用 Redis 作为共享缓存，key 形如 `cache:obj:v1:<entity>:<id>`；
每个 Manager 自己维护 key、TTL 和 JSON 反序列化逻辑。点赞、关注、关联表等低频实体已移除进程内 LRU，直接查 DB。

对象缓存默认开启，可通过 `system.yml` 关闭（关闭后所有实体回源 DB）：

```yaml
cache:
  obj:
    enabled: true
```

> 该配置项有代码默认值 `true`，不写也能正常工作；Redis 异常时缓存操作会静默降级为 DB 查询。

## 6. 会话与认证配置

登录态使用服务端 Session + `SESSIONID` Cookie，配置位于 `TechHaven.yml.tpl` 的 `auth` 节点：

```yaml
auth:
  cookie_secure: ${AUTH_COOKIE_SECURE}
  cookie_same_site: Lax
  session_absolute_ttl: 604800
```

| 配置                        | 说明                                                                 |
|-----------------------------|----------------------------------------------------------------------|
| `auth.cookie_secure`        | `SESSIONID` 是否带 `Secure`；生产必须为 `true`                       |
| `auth.cookie_same_site`     | `Lax` / `Strict` / `None`；`None` 会自动强制 `Secure`                |
| `auth.session_absolute_ttl` | 会话绝对有效期（秒），`<=0` 不限制；空闲有效期取系统设置里的会话时长 |

> 升级后已有 `prod.env` 需要补充 `AUTH_COOKIE_SECURE=true`，否则 `switch-env.sh` 会因缺少占位符而失败。

`SESSIONID` Cookie 固定带 `HttpOnly`，前端不需要也不应该读取它。

## 7. WebAuthn / Passkey 配置

Passkey 注册配置位于 `TechHaven.yml.tpl` 的 `webauthn` 节点：

```yaml
webauthn:
  rp_id: ${WEBAUTHN_RP_ID}
  rp_name: ${WEBAUTHN_RP_NAME}
  origin: ${WEBAUTHN_ORIGIN}
  timeout_ms: 60000
  user_verification: preferred
  attestation: none
  challenge_ttl: 300
  user_handle_secret: ${WEBAUTHN_USER_HANDLE_SECRET}
```

| 配置                          | 说明                                                                                                 |
|-------------------------------|------------------------------------------------------------------------------------------------------|
| `webauthn.rp_id`              | Relying Party ID，必须是注册域（如 `example.com`），**不含协议、端口和路径**；本地开发用 `localhost` |
| `webauthn.rp_name`            | 浏览器弹窗中展示的站点名                                                                             |
| `webauthn.origin`             | 前端页面的 origin（协议 + 域名 + 端口），必须与浏览器 `clientDataJSON.origin` 完全一致               |
| `webauthn.timeout_ms`         | 浏览器认证超时时间                                                                                   |
| `webauthn.user_verification`  | `preferred` 或 `required`；同时作用于 Passkey 注册和登录，`required` 要求生物识别/PIN                |
| `webauthn.attestation`        | 注册时请求的 attestation 类型，当前默认 `none`                                                       |
| `webauthn.challenge_ttl`      | challenge 在 Redis 中的有效期（秒）                                                                  |
| `webauthn.user_handle_secret` | 生成稳定 `user.id` 的 HMAC 密钥，生产环境必须配置随机值且不要随意轮换                                |

> 前后端同源时 `origin` 就是站点地址；前后端分离时，`origin` 填 **前端页面**地址，`rp_id` 填两者共同的注册域。

> Passkey 登录默认不叠加 TOTP；如果要求每次登录都做本地用户验证，请把 `webauthn.user_verification` 设为 `required`。

> 升级后已有 `prod.env` 需要补充 `WEBAUTHN_RP_ID` / `WEBAUTHN_RP_NAME` / `WEBAUTHN_ORIGIN` /
> `WEBAUTHN_USER_HANDLE_SECRET`，否则 `switch-env.sh` 会因缺少占位符而失败。

## 8. SMTP 邮件发送

SMTP 配置不走 `bin/conf/`，而是在管理后台「系统设置 → 邮件」里填写，保存在 `blog_system_settings` 表。

| 配置项            | 说明                                                                       |
|-------------------|----------------------------------------------------------------------------|
| `smtp_host`       | SMTP 服务器地址，如 `smtp.gmail.com`                                       |
| `smtp_port`       | `465`（SSL）或 `25` / `2525`（明文，通常仅限内网）                         |
| `smtp_encryption` | `ssl`（隐式 TLS，配合 465）/ `none`（明文）/ `tls`（STARTTLS，当前不支持） |
| `smtp_username`   | 发信账号；Gmail 需使用应用专用密码                                         |
| `smtp_password`   | 发信密码或应用专用密码                                                     |
| `from_email`      | 发件人地址，需与 `smtp_username` 一致，否则会被服务端拒信                  |

> **不要用 587 + `tls`**：`chen::SmtpClient` 只实现了隐式 TLS（465）和明文两种连接方式，没有实现 STARTTLS（587）。
> 用 587 会在握手阶段报 `SSL routines::wrong version number`；用 `tls` 配置调用发信接口会直接返回
> `9019 SMTP_ENCRYPTION_UNSUPPORTED`，请把端口改成 `465`、加密方式改成 `ssl`。

## 9. RPC Hub（客户端）

分布式部署时，每个 App 实例都作为客户端**主动连接** RPC Hub 并注册自己；实例之间不互连。
当前 hub 由 InkStackSearchServer 进程提供（`9010` + `args.relay: "1"`），App 只需配置 hub 地址与自身身份。

```yaml
server:
  instance_id: ${INSTANCE_ID}

rpc:
  enabled: true
  hub_address: ${RPC_HUB_ADDRESS}
  bus_id: ${RPC_BUS_ID}
  timeout_ms: 3000
  connect_timeout_ms: 1000
  reconnect_interval_ms: 5000
  heartbeat_interval_ms: 30000
```

| 配置                        | 说明                                                         |
|-----------------------------|--------------------------------------------------------------|
| `rpc.enabled`               | 是否启用 hub 客户端                                          |
| `rpc.hub_address`           | hub 地址（默认与 `SEARCH_HUB_ADDRESS` 相同）；留空视为未配置 |
| `rpc.bus_id`                | 总线身份，`<zone>.<group>.<service>.<instance>`，每段 1..255 |
| `rpc.timeout_ms`            | 经 hub 转发的请求级超时                                      |
| `rpc.connect_timeout_ms`    | 连接 hub 超时                                                |
| `rpc.reconnect_interval_ms` | 断线重连间隔                                                 |
| `rpc.heartbeat_interval_ms` | 心跳间隔（SDK 默认连续 3 次无响应断开）                      |

`bus_id` 的映射规则（代码见 `src/Config/RpcConfig.hpp`）：

- `peer_id = zone<<24 | group<<16 | service<<8 | instance`，按 hub **全局唯一**
- `func_id = group<<8 | service`，同一服务的所有实例共享，供 `GROUPID` / `BROADCAST` 路由
- `instance_id` 取第 4 段，与 `server.instance_id` 保持一致
- `service` 段约定：`1` = Search，`2` = App，`3` = Bot

> **多实例部署**：每起一个实例都必须让 `bus_id` 的第 4 段（以及 `INSTANCE_ID`）唯一，否则 hub 会拒绝重复注册。
> **多 worker / 多实例 WebSocket**：消息投递、在线人数、踢设备/撤销会话统一经 Redis 收件箱跨 worker 转发，
> 无需额外开关；请确保 `server.instance_id` 在多实例间唯一。后台任务（文章发布、计数刷盘、PR 同步、搜索快照）
> 通过 Redis 租约保证同一时刻只跑一份。
> **独立 hub**：以后把 hub 拆成独立进程时，只需要改 `RPC_HUB_ADDRESS`，App 侧不用改代码。

> 当前只落了配置与解析（`GetRpcConf()`），连接/注册（`RpcHubMgr`、`registerService`）尚未实现，因此这一节暂不影响运行行为。

> 升级后已有 `prod.env` 需要补充 `INSTANCE_ID` / `RPC_HUB_ADDRESS` / `RPC_BUS_ID`，否则 `switch-env.sh` 会因缺少占位符而失败。

## 10. GitHub API 客户端

组织仓库/PR 同步统一走 `src/Client/GitHubClient`，配置位于 `TechHaven.yml.tpl` 的 `github` 节：

```yaml
github:
  api_base: https://api.github.com
  oauth_base: https://github.com
  timeout_ms: 15000
  detail_timeout_ms: 15000
  max_attempts: 3
  backoff_ms: 300
  user_agent: InkStackServer/1.0
  per_page: 50
```

- `api_base` / `oauth_base`：GitHub API 与 OAuth 域名。
- `timeout_ms` / `detail_timeout_ms`：普通请求与 PR 详情请求超时。
- `max_attempts` / `backoff_ms`：连接失败、超时、5xx、429 的重试次数与线性退避基数。
- `per_page`：PR 列表每页条数，上限 100。
- 未配置时全部使用默认值；Token 仍按仓库存在数据库中。

## 11. 注意事项

1. **占位符命名**：统一用大写 snake_case（如 `MYSQL_HOST`、`UPLOAD_DIR`），并在 `dev.env` 与 `prod.env.example` 中保持一致。
2. **密钥不入库**：生产密码、密钥只写在 `bin/conf/prod.env`（已被 `.gitignore` 忽略），不要写进 `prod.env.example` 或提交。
3. **改模板后必须重新渲染**：直接改 `system.yml` / `TechHaven.yml` 会在下次 `switch-env.sh` 时被覆盖，请始终改 `.tpl` 和 `.env`。
4. **未定义变量会报错**：模板中出现 `${...}` 但 env 文件里没有对应变量时，脚本会终止并提示。
5. **常量不走占位符**：不会随环境变化的项直接写死，减少 env 文件噪音。
6. **`WORK_PATH` 是内置变量**：部署目录（`work_path`）可直接在 env 文件里配置（如 `prod.env` 中 `WORK_PATH=/home/web/apps/server`）；若未设置则回退为仓库根目录。
