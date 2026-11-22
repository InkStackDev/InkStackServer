# 分层架构约定

本文档约定 InkStackServer 业务代码的分层方式。目标只有一个：**每一层只做一件事，依赖方向始终向下**。

```
Servlet / Event / Logic
        │  参数校验、权限判断、拼响应、发事件
        ▼
Service
        │  业务用例、跨表编排、事务、commit 后缓存刷新
        ▼
Repository / QueryRepository
        │  单聚合读写、分页/统计查询、EntityCache
        ▼
DB / Redis / 外部 API
```

## 1. 目录与职责

| 目录                       | 职责                                                       | 禁止                                                |
|----------------------------|------------------------------------------------------------|-----------------------------------------------------|
| `src/Servlets/`            | HTTP 参数解析、登录态/权限校验、组装响应、发 EventBus 事件 | 直接调用 `DB::*Dao`、开启事务、写缓存               |
| `src/Event/`、`src/Logic/` | 事件处理、RPC 入口、Bot/Webhook 适配                       | 直接调用 `DB::*Dao`、复制业务规则                   |
| `src/Service/`             | 用例编排、跨表事务、业务校验、GitHub/外部 API 调用         | 直接使用 `DB::*Dao`、拼 SQL、操作 Servlet/HTTP 对象 |
| `src/Repository/`          | 单表/单聚合 CRUD、分页、计数、`EntityCache` 读写           | 开启事务、跨聚合业务规则、发通知/事件               |
| `src/Manager/`             | 兼容 facade、进程内只读状态、WS/路由等非 DB 能力           | 新增业务逻辑、手写 Redis 对象缓存、`ParseRow`       |
| `src/Cache/`               | 缓存模板与缓存工具                                         | 业务规则                                            |

**依赖方向**：Servlet/Event → Service → Repository。Repository 不得反向 include Service；Manager 可以调用 Repository，但
Service 不应依赖 Manager。

## 2. Repository 约定

- 所有方法显式接收 `chen::IDB::ptr conn`，不在 Repository 内部调用 `GetDB()`。
- 只操作一个聚合/一张表；关系表读取可以 join 主表，但写入只动自己的表。
- 返回 `bool` 表示“操作/查询是否成功”，与 ORM 的 `int` 错误码区分开。
- 单个实体的缓存统一用 `EntityCache`（`src/Cache/EntityCache.hpp`），key 为 `cache:obj:{area}:{id}`。
- 主键之外的映射缓存（如 `name → id`、`user_id → id`）用 `CacheIdMapping` / `GetCachedIdMapping`。
- 缓存写入只允许 `Put` / `Evict`，**不要在 DB 事务提交前刷新缓存**。

### 查询型 Repository

只读、跨表、分页/统计的查询放在 `*QueryRepository` 或 `*StatsQueryRepository`：

- `ArticleQueryRepository`：文章列表、审核列表、标签/分类分页、统计
- `CommentQueryRepository`：评论列表、回复、计数、后台统计
- `NotificationQueryRepository`：通知列表、未读数
- `AssignmentQueryRepository`、`OrganizationRepository` 内的 Query 部分
- `PrStatsQueryRepository`：PR 报表聚合

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
- 需要权限策略时（如 `CanDeleteTask`），由 Service 接收 `uid` / `org_role` / `is_platform_admin` 等上下文参数，Servlet
  负责取出这些值。

## 4. Manager 约定

- **纯数据库 facade 一律删除**，不再新增 Manager。新代码直接调用 Service / Repository。
- 仍然保留的 Manager 只允许是非数据库能力：
  - `WSConnectionManager`：进程内 WS 连接状态
  - `SystemSettingsManager`：进程内设置快照 + 版本校验
  - `ErrorCodeManager`：`errors.json` 内存表与 i18n
  - `SessionManager`：session TTL 计算
  - `NotificationManager` / `MessageManager`：目前仅剩 WS 推送入口，后续迁入 `MessageRouter` 后删除
- 已删除的纯 facade（示例）：
  - 基础：`BadgeManager`、`FaqManager`、`FeedbackManager`、`ResourceManager`、`UserOAuthManager`、
    `OrganizationApplyManager`、`UserAIConfigManager`、`AssignmentUserRelManager`
  - 组织：`OrganizationManager`、`OrganizationUserRelManager`、`OrganizationRepoManager`、`OrganizationRepoPrManager`
    （组织角色/状态枚举已迁至 `src/Domain/OrganizationTypes.hpp`）
- 现有 Manager 中不得再出现：
  - `static ... ParseRow(...)`
  - 手写 `RedisCacheGet` / `RedisCacheSetex` / JSON codec
  - 分页/统计 SQL

## 5. 缓存与多 worker

- 对象缓存：`EntityCache`，提交后 `Put`，删除/软删除后 `Put`（软删除对象仍需可读）或 `Evict`。
- 列表/计数缓存：使用版本号而不是直接删 key：
    - `cmt:art:ver:{article_id}`、`notif:ver:{user_id}` 等
    - 写操作后 `INCR` 版本号，key 自动切换到新版本
- 跨 worker 一致性：
    - 不要依赖进程内单例保存可变业务状态；需要共享的放 Redis/MySQL。
    - 系统设置通过 `settings:version` 做秒级校验后重载。
    - 后台任务用 `JobRunner` + `RedisLease`，保证多 worker 只跑一份。
- 长连接只留在接入 worker，跨 worker 投递走 `MessageRouter`，不要把 WS 连接代理成 RPC。

## 6. 新增功能检查清单

1. `dbproxy/xml/` 定义表结构，执行 `make orm`
2. `src/Repository/` 新增 `XxxRepository`（单表 + `conn` + `EntityCache`）
3. 列表/统计放 `XxxQueryRepository`
4. `src/Service/` 新增 `XxxService`，处理事务、业务校验、缓存刷新
5. `src/Servlets/` 只做参数/权限/响应/事件
6. 更新 `src/Include/Managers.hpp`（如需兼容旧入口）
7. 新错误码同时更新 `src/ErrorCodes.hpp` 与 `errors.json`
8. 涉及跨 worker 状态时，确认已使用 Redis/JobRunner/MessageRouter

## 7. 静态检查清单

提交前建议执行：

```bash
# Repository 之外不应出现网关 DAO
rg 'DB::[A-Za-z]+InfoDao::' src --glob '!src/Repository/**' --glob '!src/DB/DBManager.cc'

# Manager 不应再有手写缓存/ParseRow
rg 'ParseRow|RedisCacheGet|RedisCacheSetex' src/Manager

# 格式
git diff --check
```
