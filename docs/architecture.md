# 分层架构约定

本文档约定 InkStackServer 业务代码的分层方式。目标只有一个：**每一层只做一件事，依赖方向始终向下**。

```
Servlet / Event / Logic / RPC
        │  认证、粗粒度路由鉴权、参数校验、拼响应、发事件
        ▼
Service
        │  业务用例、命令级鉴权、跨表编排、事务、commit 后缓存刷新
        ├──────────────────────────────┐
        ▼                              ▼
Repository / QueryRepository        Client（出站适配器）
        │  数据范围、分页/统计、缓存        │  超时/重试/限流/分页/DTO
        ▼                              ▼
    DB / Redis                  GitHub / LLM / Search / SMTP
```

入口层只回答“是谁、能不能进这个路由”；“能看哪些行”由 QueryRepository 的数据范围参数决定；“能不能执行这个命令”由
Service 的策略决定。外部网络调用统一走 Client，Repository 不直接访问外部 API。

## 1. 目录与职责

| 目录                       | 职责                                                               | 禁止                                                     |
|----------------------------|--------------------------------------------------------------------|----------------------------------------------------------|
| `src/Servlets/`            | HTTP 参数解析、登录态/粗粒度路由鉴权、组装响应、发 EventBus 事件   | 直接调用 `DB::*Dao`、开启事务、写缓存、成员/角色数据过滤 |
| `src/Event/`、`src/Logic/` | 事件处理、RPC 入口、Bot/Webhook 适配                               | 直接调用 `DB::*Dao`、复制业务规则                        |
| `src/Service/`             | 用例编排、命令级鉴权、跨表事务、业务校验、调用 Repository / Client | 直接使用 `DB::*Dao`、拼 SQL、操作 Servlet/HTTP 对象      |
| `src/Access/`              | 访问者身份到数据范围/策略的解析                                    | 拼 SQL、返回 errcode、依赖 Servlet                       |
| `src/Repository/`          | 本地存储取数、数据范围、分页、计数、`EntityCache` 读写             | 调用外部 API、开启事务、跨聚合业务规则、发通知/事件      |
| `src/Client/`              | 外部系统适配、网络治理、DTO 映射、错误分类                         | 访问 DB、开启事务、业务规则、返回 `Result`/errcode       |
| `src/Manager/`             | 兼容 facade、进程内只读状态、WS/路由等非 DB 能力                   | 新增业务逻辑、手写 Redis 对象缓存、`ParseRow`            |
| `src/Cache/`               | 缓存模板与缓存工具                                                 | 业务规则                                                 |

**依赖方向**：Servlet/Event → Service → Repository / Client。Repository 不得反向 include Service 或 Client；Client 不得
include Repository；Manager 可以调用 Repository，但 Service 不应依赖 Manager。

**数据权限分层**：

- `src/Domain/AccessTypes.hpp`：`Viewer` / `OrgScope` 纯数据类型，不依赖 HTTP、`Result`、`ErrorCodes`。
- `src/Access/AccessScopeResolver`：一次查询把用户解析成 `OrgScope`；管理员得到 `All()`，普通用户得到授权组织集合。
- `src/Repository/*QueryRepository`：消费 `OrgScope` 并在 SQL WHERE 中应用；空 scope 必须查不到数据，不能退化成全量。
- `src/Access/AccessPolicy`：单实体查看/命令策略，供 Service 使用。
- 入口层不直接查成员关系，也不把 `target_orgs` / `is_admin` 传给 Repository。

## 2. Repository 约定

- 所有方法显式接收 `chen::IDB::ptr conn`，不在 Repository 内部调用 `GetDB()`。
- 只操作一个聚合/一张表；关系表读取可以 join 主表，但写入只动自己的表。
- 返回 `bool` 表示“操作/查询是否成功”，与 ORM 的 `int` 错误码区分开。
- 单个实体的缓存统一用 `EntityCache`（`src/Cache/EntityCache.hpp`），key 为 `cache:obj:{area}:{id}`。
- 主键之外的映射缓存（如 `name → id`、`user_id → id`）用 `CacheIdMapping` / `GetCachedIdMapping`。
- 缓存写入只允许 `Put` / `Evict`，**不要在 DB 事务提交前刷新缓存**。
- 面向用户的列表/统计查询必须接收 `OrgScope`（或领域专用 scope），由 Repository 应用数据范围；不再接收调用方算好的
  `org_ids` / `is_admin`。
- 空 scope 视为无权限，必须追加恒假条件；禁止 `if (!org_ids.empty())` 这种“空 = 全量”的 fail-open 写法。
- Repository 只访问本地存储（MySQL / Redis），不得调用外部 HTTP / RPC / SMTP。

### 查询型 Repository

只读、跨表、分页/统计的查询放在 `*QueryRepository` 或 `*StatsQueryRepository`：

- `ArticleQueryRepository`：文章列表、审核列表、标签/分类分页、统计
- `CommentQueryRepository`：评论列表、回复、计数、后台统计
- `NotificationQueryRepository`：通知列表、未读数
- `AssignmentQueryRepository`、`OrganizationRepository` 内的 Query 部分
- `PrStatsQueryRepository`：PR 报表聚合
- `RdStatsQueryRepository`：需求/缺陷/任务趋势与概览统计

报表 SQL 不允许出现在 Servlet 中。

## 3. Service 约定

- Service 是唯一允许开启事务的地方：`auto trans = db->openTransaction(); ... trans->commit();`
- 跨表写入必须在同一事务中完成，例如：
    - 创建组织 + 创建管理员成员关系
    - 通过组织申请：建组织 + 建成员关系 + 更新申请状态
    - 发送私信：写消息 + 更新会话未读数
- 事务失败要 `rollback()`，并且不要把未提交的对象写进缓存。
- commit 成功后再 `Repository::Put` / `Evict` / 失效计数或列表版本号。
- 业务错误用 `enum class XxxResult` 表达，由 Servlet 映射成 `errcode`，不要把 HTTP/`Result` 传进 Service。
- 需要权限策略时，Service 接收 `Viewer`，并通过 `AccessScopeResolver` 解析 `OrgScope`；不要让 Servlet 传
  `org_role` / `is_platform_admin` / `target_orgs`。
- 单实体读取用 `AccessPolicy` 判断可见性；列表/统计把 `OrgScope` 传给 QueryRepository，由 SQL 应用数据范围。

## 4. Client（外部系统适配）约定

- 所有外部 HTTP / RPC / SMTP 调用放在 `src/Client/`，Service 只调用 Client，不直接使用 `HttpConnection` / `RpcClient` /
  `SmtpClient`。
- Client 负责：base URL、认证头、超时、重试、退避、限流、分页、JSON → DTO、错误分类、耗时指标、日志脱敏。
- Client 不负责：访问 DB、开事务、业务规则、权限判断、返回 `Result` / errcode。
- Client 返回 `ClientResult<T>`（或 `bool + 出参`），Service 负责把 `ClientError` 映射成领域结果，入口再映射成 errcode。
- 只在幂等请求上重试；流式 AI 只在收到第一个 token 前重试。
- **Client 调用期间不得持有 DB 连接或事务**。Service 的顺序固定为：读本地上下文 → 释放连接 → 调 Client →
  重新拿连接/开事务 →
  写本地 → commit 后刷缓存。
- 外部 API 的 JSON 不越过 Client 边界，Service 不直接读厂商字段。

## 5. Manager 约定

- **纯数据库 facade 一律删除**，不再新增 Manager。新代码直接调用 Service / Repository。
- 仍然保留的组件只允许是非数据库能力（后续可逐步改名为 Registry / Cache / Util）：
  - `WSConnectionManager`：进程内 WS 连接状态
  - `SystemSettingsManager`：进程内设置快照 + 版本校验
  - `ErrorCodeManager`：`errors.json` 内存表与 i18n
  - `SessionManager`：session TTL 计算
  - `CacheUtil`：Repository 共用缓存工具
- 已删除的纯 facade（示例）：
  - 基础：`BadgeManager`、`FaqManager`、`FeedbackManager`、`ResourceManager`、`UserOAuthManager`、
    `OrganizationApplyManager`、`UserAIConfigManager`、`AssignmentUserRelManager`
  - 组织：`OrganizationManager`、`OrganizationUserRelManager`、`OrganizationRepoManager`、`OrganizationRepoPrManager`
    （枚举已迁至 `src/Domain/OrganizationTypes.hpp`）
  - 内容：`ArticleManager`、`CategoryManager`、`LabelManager`、`ArticleCategoryRelManager`、`ArticleLabelRelManager`、
    `ArticlePraiseRelManager`、`CommentManager`、`CommentPraiseRelManager`
    （枚举已迁至 `src/Domain/ArticleTypes.hpp` / `src/Domain/CommentTypes.hpp`）
  - RD：`TaskManager`、`BugManager`、`RequirementManager`、`AssignmentManager`、`AssignmentOrganizationRelManager`
    （枚举已迁至 `src/Domain/RdTypes.hpp`）
  - 账号/记录：`PasskeyManager`、`BackupRecordManager`、`ExportRecordManager`、`UserFollowRelManager`
  - 用户/私信/通知：`UserManager`、`MessageManager`、`NotificationManager`
- 现有 Manager 中不得再出现：
  - `static ... ParseRow(...)`
  - 手写 `RedisCacheGet` / `RedisCacheSetex` / JSON codec
  - 分页/统计 SQL

## 6. 缓存与多 worker

- 对象缓存：`EntityCache`，提交后 `Put`，删除/软删除后 `Put`（软删除对象仍需可读）或 `Evict`。
- 列表/计数缓存：使用版本号而不是直接删 key：
    - `cmt:art:ver:{article_id}`、`notif:ver:{user_id}` 等
    - 写操作后 `INCR` 版本号，key 自动切换到新版本
- 跨 worker 一致性：
    - 不要依赖进程内单例保存可变业务状态；需要共享的放 Redis/MySQL。
    - 系统设置通过 `settings:version` 做秒级校验后重载。
    - 后台任务用 `JobRunner` + `RedisLease`，保证多 worker 只跑一份。
- 长连接只留在接入 worker，跨 worker 投递走 `MessageRouter`，不要把 WS 连接代理成 RPC。

## 7. 新增功能检查清单

1. `dbproxy/xml/` 定义表结构，执行 `make orm`
2. `src/Repository/` 新增 `XxxRepository`（单表 + `conn` + `EntityCache`）
3. 列表/统计放 `XxxQueryRepository`，签名接收 `OrgScope` 并在 SQL 中应用数据范围
4. `src/Service/` 新增 `XxxService`，处理事务、命令级鉴权、缓存刷新
5. 外部调用放 `src/Client/`，Service 只调用 Client
6. `src/Servlets/` 只做参数/粗粒度路由鉴权/响应/事件
7. 更新 `src/Include/Managers.hpp`（如需兼容旧入口）
8. 新错误码同时更新 `src/ErrorCodes.hpp` 与 `errors.json`
9. 涉及跨 worker 状态时，确认已使用 Redis/JobRunner/MessageRouter

## 8. 静态检查清单

提交前建议执行：

```bash
# Repository 之外不应出现网关 DAO
rg 'DB::[A-Za-z]+InfoDao::' src --glob '!src/Repository/**' --glob '!src/DB/DBManager.cc'

# Manager 不应再有手写缓存/ParseRow
rg 'ParseRow|RedisCacheGet|RedisCacheSetex' src/Manager

# 入口层不应直接查成员关系/拼报表 SQL
rg 'GetByOrgAndUser|OrganizationUserRelRepository::ListByUser' src/Servlets
rg 'QueryBuilder|executeCount|queryPairs' src/Servlets

# 外部调用应集中在 Client
rg 'HttpConnection|HttpClient|SmtpClient|RpcClient' src/Servlets src/Service src/Repository --glob '!src/Client/**'

# 格式
git diff --check
```
