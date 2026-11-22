# Changelog

All notable changes to this project will be documented in this file.

## 未发布

### 新增

- 新增 `Viewer` / `OrgScope` / `AccessScopeResolver` / `AccessPolicy`：入口只解析身份与粗粒度路由权限，组织数据范围统一由
  Service 解析、QueryRepository 在 SQL 中应用；空范围按无权限处理，不再退化成全量。
- 新增 `RdStatsQueryRepository` / `RdAnalyticsService`：研发趋势、概览与 PR 分布统计从 Servlet 迁移到 Service +
  QueryRepository，移除入口层报表 SQL、成员关系查询与内存分页；`PrStatsQueryRepository` 改为显式接收 `OrgScope`。
- 新增 `RdListQueryRepository`：任务/需求/缺陷列表改为 SQL 数据范围 + 过滤 + 分页；入口不再按组织循环取全量再内存过滤，
  任务列表同时保留“普通成员只看自己创建/负责的任务”的规则。
- RD 任务/需求/缺陷的保存与批量删除改为 Service 接收 `Viewer`：组织范围解析、创建/编辑/删除权限判断统一在
  `TaskService` / `RequirementService` / `BugService` 内完成，Servlet 不再查询成员关系或传 `org_role` /
  `is_platform_admin`。平台管理员创建/编辑 RD 项时不再要求目标组织成员关系，与列表/详情/删除保持一致。
- RD 组织列表、组织成员列表、平台准入检查改为 Service + `Viewer` / `OrgScope`：成员关系查询从入口移除，
  `AccessPolicy::CanAccessPlatform` 统一平台准入判断。
- 组织创建/删除、申请审核、加入审核、踢人、角色切换改为 Service 接收 `Viewer`：管理员校验、成员身份、
  `DEV_LEAD`/`ORG_ADMIN` 管理权限与角色切换范围统一在 `OrganizationService` 内完成，入口不再查询操作者成员关系。
- 组织详情、成员分页、组织作业列表改为 Service 读模型：`GetDetail` / `ListMemberPage` / `ListAssignments`，
  入口不再直接调用 `OrganizationUserRelRepository` / `AssignmentRepository`；待审核成员列表要求管理权限。
- 组织仓库与 PR 入口改为 Service 权限模型：仓库增删/Token/同步/列表走 `OrganizationRepoService` 的 `Viewer` +
  `AccessPolicy`，PR 列表/删除/同步复用 `CheckOrgAccess` / `CheckRepoAccess`；入口不再查询操作者成员关系，
  已退出/已拒绝关系不再能管理仓库或 PR。
- 新增 `src/Client/` 出站适配层与 `GitHubClient`：GitHub 仓库元数据、PR 列表/详情/评审统一在 Client 内完成
  超时、重试、退避、限流识别、JSON → DTO 和错误分类；`OrganizationRepoService` / `OrganizationRepoPrService`
  不再直接使用 `HttpConnection` 和厂商 JSON。新增 `github` 配置节（默认值兜底）。
- GitHub OAuth 登录改为 `GitHubClient::ExchangeOAuthCode` / `GetOAuthUser` / `GetPrimaryEmail`：
  `UserOAuthGithubCallbackServlet` 不再直接发 HTTP、解析厂商 JSON 或打印响应 body；state 校验与本地用户绑定逻辑留在入口/Service。
- 新增 `LlmClient` / `ArticleAIService`：文章 AI 总结的请求构建、流式解析、首 token 前重试与错误分类下沉到 Client/Service，
  `SSEStreamParser` 改为通用回调；`ArticleAISummaryServlet` 只负责 SSE 会话、参数和结果事件。`ai` 配置增加
  `timeout_ms` / `max_attempts`。
- 新增 `SearchClient`，把 SearchServer 的 RPC 连接、超时、stats、query、upsert/remove、快照 RPC 从
  `SearchSync` 拆出；`SearchSync` 只保留 Redis outbox、批量调度、快照组装和失败重试。
- 新增 `MailClient`：SMTP 连接、发送、耗时日志与错误分类从 `EventUserSendCode` 移入 Client；
  Event 只负责校验邮件数据和触发发送。
- 新增 `ArticleSearchService`：公共搜索的“只查已发布文章”、搜索调用、文章/作者数据组装从
  `ArticleSearchServlet` 移入 Service，入口只负责参数与响应。
- 新增 `RdListService` / `RdAccessService` / `RdTicketService` 与 RD 详情 Service：`src/Servlets/Rd/` 已不再
  出现 `GetDB()`、Repository 或 `AccessScopeResolver` 直接调用；列表、详情、准入检查和“我的单据”均由 Service
  完成连接获取与数据范围解析。
- 新增 `AdminCommentService` / `AdminArticleService` / `AdminAssignmentService` / `AdminUserService`：
  `src/Servlets/Admin/` 已不再出现 `GetDB()`；管理端统一继承 `BlogAdminServlet`，评论/文章/作业/用户/组织
  列表与统计由 Service 完成 DB 访问。
- 新增 `UserQueryService` / `UserStatsService` 与 `UserAIConfigService::Get`：`src/Servlets/User/` 已不再出现
  `GetDB()`；用户资料、关注/粉丝/互关、用户组织/作业、统计、成就、AI 配置均通过 Service 获取数据。
- 新增 `ArticleQueryService` / `CommentQueryService`：`src/Servlets/Article/` 已不再出现 `GetDB()`；
  文章列表/详情/点赞、评论树/回复、评论创建/修改/点赞的 DB 访问与数据组装均下沉到 Service。
- 新增 `DashboardService` / `DatabaseAdminService` / `RecordService` / `ResourceService` / `CategoryQueryService` /
  `LabelService::List` / `NotificationService` 查询方法 / `AssignmentService` 读模型：`src/Servlets/`
  全目录已不再出现 `GetDB()`；备份/导出、仪表盘、通知、私信、作业、分类、标签、资源、认证等入口均通过 Service
  获取数据库连接。
- 新增统一 WebSocket 消息路由 `MessageRouter`：统一走 Redis 收件箱跨 worker 投递消息、在线人数以及踢设备/撤销会话指令，连接所在
  worker 直接投递作为快路径，长连接本身不经过 RPC。
- 新增后台任务执行器 `JobRunner` 与 Redis 租约 `RedisLease`：文章定时发布/计数刷盘、组织仓库与 PR 同步改为定时任务，多
  worker 下同一任务只跑一份。
- 搜索全量重建和单条 PR 同步增加 Redis 租约，避免多 worker 重复执行。
- 新增 `UserRepository` / `UserService` / `EntityCache` 样板：Repository 只做单表读写并显式接收 `conn`，Service 负责业务校验、事务和
  commit 后缓存失效；用户注册/创建、GitHub OAuth 用户创建、个人资料修改、2FA 启用/关闭/恢复码登录、密码设置、软删除/恢复、管理端资料更新已迁移到该路径。
- 新增 `ArticleRepository` / `ArticleService` 样板，文章对象缓存收敛到 `EntityCache`
  ，文章创建、更新、删除、状态切换、审核、发布已迁移（文章 + 分类/标签关系、事务、缓存、搜索同步、定时发布/日历清理、审核/状态事件统一在
  Service 内完成）。
- 新增 `ArticleScheduleService`，定时发布、发布日历缓存从 `ArticleManager` 拆出。
- 新增 `ArticleCounterService`，浏览/点赞/收藏计数、内存脏集合与刷盘从 `ArticleManager` 拆出。
- 新增 `ArticleQueryRepository`，文章列表/审核列表/后台分页/标签与分类分页/统计/上一篇下一篇查询从 `ArticleManager` 拆出。
- 新增 `CommentRepository` / `CommentService` 样板，评论对象缓存收敛到 `EntityCache`，评论创建/删除已迁移。
- 新增 `CommentQueryRepository` / `CommentPraiseRepository`：评论列表、回复、计数、后台分页与统计查询，以及评论点赞关联读写从
  `CommentManager` / `CommentPraiseRelManager` 拆出；评论修改、批量审核/删除、点赞切换迁移到 `CommentService`，写操作后失效
  对应的文章列表/计数与全局统计缓存。
- 新增 `NotificationRepository` / `NotificationQueryRepository` / `NotificationService`：通知读写、列表/计数/未读数查询、已读标记、
  广播关闭与过期清理从 `NotificationManager` 拆出；用户级缓存改为版本号失效，批量已读补上归属校验。
- 新增 `ConversationRepository` / `ConversationMessageRepository` / `ConversationService`：私信会话与消息读写从
  `MessageManager`
  拆出，发消息改为单事务更新消息与会话未读数，会话创建依赖唯一索引 + 回查避免跨 worker 重复插入。
- 新增 `AssignmentRepository` / `AssignmentRelRepository` / `AssignmentQueryRepository` / `AssignmentService`：作业创建/更新、
  组织布置、批量删除从 Servlet 与 `Assignment*Manager` 拆出；批量删除不再全表扫后再过滤，改为按 ID 读取并在事务内软删除。
- 新增 `OrganizationRepository` / `OrganizationUserRelRepository` / `OrganizationApplyRepository` /
  `OrganizationService`：
  组织创建/更新、批量删除、加入/审核/踢人/角色调整、组织申请与审核从 Servlet 与 `Organization*Manager` 拆出；组织创建与
  申请人成员关系、申请通过后的组织 + 关系 + 申请状态更新改为单事务提交。
- 新增 `TaskRepository` / `BugRepository` / `RequirementRepository` 与对应 Service：任务/缺陷/需求的保存与批量删除从
  `Rd*Servlet` 拆出，删除改为按 ID 过滤 + 事务软删除，权限判断保留在 Service 入参中。
- 新增 `CategoryRepository` / `LabelRepository` / `ArticleRelRepository` 与 `CategoryService` / `LabelService` /
  `ArticleCategoryService`：分类/标签的创建、级联删除、文章分类关联更新从 Servlet 与 Manager 拆出；分类删除改为在 Service
  内递归子分类并事务提交，文章分类更新修复了响应里误返回关联记录 ID 的问题。`ArticleService` 与搜索快照的分类/标签关联查询
  也改为走 `ArticleRelRepository`。
- 新增 `BadgeRepository` / `FaqRepository` / `FeedbackRepository` / `BackupRecordRepository` / `ExportRecordRepository`
  与
  `FaqService` / `FeedbackService`：徽章、常见问题、反馈、备份/导出记录读写从 Manager 与 Servlet 拆出；FAQ 转换、反馈提交、
  过期记录物理清理改为走 Repository / Service。
- 新增 `OrganizationRepoRepository` / `OrganizationRepoService`：组织仓库的增删、Token 更新、GitHub 元数据同步从
  `OrganizationRepoManager` 与 Servlet 拆出；PR 同步相关的仓库状态回写与 `github_full_name` 反查也改为走该 Repository。
- 新增 `OrganizationRepoPrRepository`：PR 单表读写、按仓库/组织/用户分页与对象缓存从 `OrganizationRepoPrManager` 拆出；
  PR webhook 写入、后台同步、删除与单条 PR 同步的持久化统一走 Repository。
- 新增 `PasskeyRepository` / `ResourceRepository` / `UserAIConfigRepository` 与 `UserAIConfigService`：Passkey 凭据、资源元数据、
  用户 AI 配置的读写从 Manager 拆出，AI 配置保存仍保留“按 user_id 回查主键后刷新缓存”的行为。
- 新增 `OrganizationRepoPrService`：PR 全量/单条同步、GitHub 状态机、限流与重试、仓库 PR 同步状态回写从
  `OrganizationRepoPrManager` 拆出，Manager 只保留查询/缓存 facade。
- 新增 `ArticlePraiseRelRepository` / `UserFollowRelRepository` 与对应 Service：文章点赞、用户关注的对象缓存与读写从
  Manager
  拆出，Manager 中最后两处手写 Redis 缓存与 `ParseRow` 移除。
- 新增 `PrStatsQueryRepository`：PR 按天聚合、评审通过率、时间范围内创建数等报表查询从 `RdTrendServlet` /
  `RdPrDistributionServlet` 拆出，报表 SQL 与业务读写彻底分离。
- 清理 Servlet 中最后的直接 DAO 调用（评论/文章计数、用户详情、登录密码哈希升级）。
- 新增统一 WebSocket 入口 `/ws/v1/session`，一个设备一条长连接，复用通知、在线人数、私信和系统消息通道。
- 新增 `channel` 下行字段（`system` / `notification` / `presence` / `chat`），客户端按通道分发。
- 新增 `WSConnectionManager`：按 `device_id` 管理连接、按用户索引、在线人数去重、连接级发送串行化。
- 新增 `blog_ws_online_devices` 指标，统计 WebSocket 连接数。
- 新增 RPC Hub 客户端配置（`rpc.enabled` / `hub_address` / `bus_id` / 超时），并集中到 `src/Config/RpcConfig.hpp`；
  `bus_id` 为 `<zone>.<group>.<service>.<instance>` 四段位，用于推导 hub 侧的 `peer_id` 与 `func_id`。
  本轮只落配置与解析，连接/注册逻辑后续实现。
- 新增多实例部署打包：`make cluster ENV=prod INSTANCES=3` →
  `scripts/cluster/package-cluster.sh` 产出 `techhaven-cluster-<version>-<arch>-<ts>.tar.gz`，
  包内 `instances/<name>/` 已按实例生成好 `conf/` 与 `instance.env`（端口、instance_id、bus_id、日志/pid 彼此独立）。
  程序目录默认 `/opt/techhaven`，**只有日志**单独放在 `/data/apps/instances/<name>/logs`
  （可用 `--deploy-path` / `--log-root` 调整）；
  另含 `clusterctl.sh`、`render-instance.sh`、Nginx 示例与多实例说明；`make dist` 现在直接产出集群包。
- `Build & Package` 去掉触发时的打包输入参数，统一读仓库 Variables（`PKG_ENV` / `CLUSTER_INSTANCES` /
  `DEPLOY_PATH` / `CLUSTER_LOG_ROOT`）；`Deploy` 只保留 `mode` 输入。
- CI 只发行集群包：`Build & Package` 产出 `techhaven-cluster-<tag>-<arch>.tar.gz`（含 `.sha256`），
  支持 `instances` / `deploy_path` / `log_root` 输入；`Deploy` 改为集群部署（`cold` 全量 / `hot` 只换
  `module`、`errors.json`、`locale`、`scripts` 后滚动重启 / `stop`），不再下载单实例包。
- `Deploy` 新增 `module` 模式：只替换 `module/*.so`（旧文件保留为 `*.so.prev`），不重启实例，
  由运维自行 `clusterctl.sh reload <name>` 做灰度；确认后再全量滚动重启。

### 变更

- **不兼容变更**：移除 `/ws/v1/notification`、`/ws/v1/presence`、`/ws/v1/messages`，统一改为 `/ws/v1/session`。
- **不兼容变更**：所有 WebSocket 下行 JSON 顶层新增 `channel` 字段。
- **不兼容变更**：同一 `device_id`（`SHA256(SESSIONID)`）只保留一条连接，后连接顶掉前连接并返回 `replaced` 帧。
- 私信 `message_ack` 只回发起设备；消息与已读回执推送给对端所有设备。
- 普通用户可连接统一网关接收通知与在线人数，但私信帧返回 `ACCESS_DENIED`。
- 登出、踢设备、撤销单会话、撤销全部会话、重新签发会话时联动关闭对应 WebSocket。
- `blog_online_users` 改为按 `uid` 去重的在线用户数。
- 业务侧配置统一收敛到 `src/Config/`（`AppConfig` / `AuthConfig` / `SearchConfig` / `AIConfig`），各模块不再各自声明
  `Config::Lookup`；新增配置项需在该目录登记并更新 key 对照表。
- 多实例部署适配：`RedisLease` 改为 **fail-closed**（Redis 不可用时跳过本轮任务，日志每 60s 节流），
  `JobRunner::Job` 新增 `use_lease`，默认租约任务与“每实例各自执行”的任务分离。
- 文章浏览/点赞/收藏计数改为**进程内增量 + `views = views + ?` 增量写库**，刷盘任务不再占用租约，避免多实例
  用绝对值互相覆盖；刷盘成功后 `Evict` 对象缓存，让缓存重新对齐 DB 真值。新增 `docs/multi-instance.md`。
- 多实例配置补充 `server.process_name`：每个实例进程标题为 `techhaven-<实例名>: master process` /
  `techhaven-<实例名>: worker process`，`ps -ef | grep techhaven` 即可区分实例；单实例默认 `techhaven`
  （`switch-env.sh` 兜底，可用 `PROCESS_NAME` 覆盖）。
- 配置模板整理：`system.yml.tpl` 删除已废弃的 rpc server 项（业务只作为 hub 客户端）、`fox_thread.redis.name` 与
  `redis.desc` 等未读取项；接入端口改为 `HTTP_PORT` / `WS_PORT`，MySQL 显式 `pool`；`TechHaven.yml.tpl` 补齐
  `server.drain_timeout_ms` / `housekeeping_interval_ms` / `worker_ready_timeout_ms` / `worker_restart_interval_ms`。

### 修复

- 修复验证码邮件始终按隐式 TLS 连接 SMTP 服务的问题：改为按 `smtp_encryption` 选择连接方式，不再写死 SSL；对当前 SDK 不支持的
  STARTTLS（587 + `tls`）直接返回 `9019 SMTP_ENCRYPTION_UNSUPPORTED`，不再静默失败。
- 修复异步发信事件在 `SmtpClient::send` 返回空指针时解引用导致 worker SIGSEGV 的问题，并补充连接/发信耗时与失败原因日志。
- 修复访问日志 `GetRemoteIP` 未判空 `session` 可能导致空指针崩溃的问题。
- 崩溃日志现在记录崩溃时间、线程 ID、`si_code`、出错 PC/SP/FP、模块基址与 PC 偏移、栈内容，可在帧丢失时仍用 `addr2line` 定位。
- 系统设置的写路径（保存设置、上传站点图片）改为先复制再整体替换，避免与 `/api/v1/site/settings` 等读请求并发改写同一个
  `std::string`。
- 系统设置跨 worker 生效：本地快照按 Redis 版本号最多 1 秒校验一次，任一 worker 保存设置后其他 worker 自动重载。
- 修复 `server.work_path` / `server.data_path` 使用单参 `Config::Lookup`（只查不注册）时，依赖跨编译单元静态初始化顺序、
  顺序不利会拿到空指针并崩溃的隐患；现在统一由 `src/Config/AppConfig.cc` 带默认值注册。
- 修复模块重写后丢失的启动初始化：`onLoad` 恢复 `FoxThreadMgr::init/start` + `RedisMgr::init`、`I18n::Init`、
  `ErrorCodeMgr::load`，`onServerReady` 恢复 `Init(...)` 服务指标注册，`onTick` 恢复 `SampleRates()` 与
  `SessionDataMgr::check()`，`onUnload` 补 `RedisMgr::freeAll()`。
- 访问日志改用独立的 `access` logger（之前写到 root.log）。
- 日志文件开启按天轮换：`time_rotate: day` + `retention_days: 14`，文件名形如
  `root_2026-11-23.log` / `access_2026-11-23.log`，避免单文件无限增长。
- 修复 `BlogLoginedServlet::handlePre` 会话判断写反导致**带 session cookie 的请求全部返回
  `1101 NOT_LOGIN`**（前端表现为“登录后立刻过期/请求被取消”）的问题。
- 打包脚本（`collect.sh` / `package-cluster.sh`）在 `locale/*.mo` 缺失时自动尝试 `ninja i18n` /
  `scripts/i18n.py build`，仍失败则提示先安装 gettext 并执行 `make i18n`。
- 恢复首次部署的超级管理员引导 `UserService::EnsureSuperAdmin()`：按 `admin` 配置创建，账号/邮箱已存在则跳过，
  `ADMIN_PASSWD` 留空则跳过；多实例并发引导依赖 `user` 表唯一索引，按“已存在”处理。

### 移除

- 移除旧 WebSocket 端点、旧通知/在线状态/私信连接表及相关管理方法。
- 移除服务端文件上传接口：`/api/v1/file/upload`、`/api/v1/upload/*`、`/api/v1/admin/settings/upload`，
  以及 `FileUploadServlet`、`ChunkUploadServlet`、`SystemSettingsUploadServlet`；后续文件存储统一走对象存储，
  站点图标/Logo/Favicon 先通过 `/api/v1/admin/settings` 填写 URL。`chunk_upload` 表不再创建，历史资源元数据保留在 `resource` 表。

### 升级注意

- 前端需要切换到 `/ws/v1/session`，并按 `channel` 字段分发下行消息。
- 同设备多标签页不再各自保留连接，后打开的标签页会导致前一个连接收到 `replaced`。

## v1.1.0 (2026-10-26)

本版本完成认证体系升级：登录态统一为服务端会话 + HttpOnly `SESSIONID` Cookie，新增 Passkey / WebAuthn、邮箱验证码恢复与恢复码流程，并移除旧
`S_UID` / `S_TOKEN` 认证链路。同时引入 Redis 对象缓存、崩溃保护和完整国际化。

### 新增

- 新增 Passkey 认证：
  - Passkey 注册（`/api/v1/auth/passkey/register/*`）
  - Passkey 登录（`/api/v1/auth/passkey/login/*`），支持 discoverable credential
  - Passkey 列表与删除（`/api/v1/auth/passkey/list`、`/api/v1/auth/passkey/delete`）
  - 支持 ES256、EdDSA、RS256 COSE 公钥验签
- 新增邮箱验证码恢复登录（`/api/v1/auth/email/recovery`）：
  - 邮箱验证码一次性校验
  - 恢复成功后撤销全部旧会话并签发新 `SESSIONID`
  - 验证码错误次数限制
- 新增服务端会话体系：
  - Redis 会话存储，Cookie 只保存随机 `SESSIONID`
  - 会话列表、当前设备标识与远程下线
  - 会话空闲续期与绝对过期时间
- 新增 Redis 对象缓存，替代进程内 LRU，缓存用户、文章、评论、通知、组织等高频实体。
- 新增国际化支持（`zh_CN` / `en`），覆盖错误码、后端消息和业务枚举文案。
- 新增崩溃处理器，记录崩溃日志；补充空指针、越界和异常数据防护。
- 新增 Passkey、WebAuthn、邮箱恢复相关错误码（`2029` - `2039`）。
- 新增 `user_passkey` 表，用于保存 WebAuthn 凭据、签名计数和备份状态。

### 优化

- 登录、2FA、Passkey、GitHub OAuth、邮箱恢复统一通过 `AuthService::IssueSession` 签发会话。
- 2FA 登录临时凭证改为 Redis 一次性凭证，验证后立即失效。
- GitHub OAuth 登录接入新的服务端会话体系。
- WebSocket 握手改为复用 `SESSIONID` Cookie，不再从 URL query 读取 `uid/token/token_time`。
- 优化管理员账号创建、删除、恢复、重置密码和权限校验逻辑。
- 超级管理员初始密码不再使用默认值，改为从 `ADMIN_PASSWD` 注入。
- WebAuthn COSE 密钥构建迁移到 OpenSSL 3.0 `EVP_PKEY_fromdata` / `OSSL_PARAM` API，移除低层 EC/RSA 弃用接口。
- 优化错误码国际化、语言解析和错误消息输出。
- 优化 RSS 读取、崩溃日志目录创建等运行稳定性细节。

### 修复

- 修复崩溃日志目录不存在时无法写入日志的问题。
- 修复部分 Servlet / Manager 在空指针、空集合和越界数据下的异常风险。
- 修复运行目录未自动创建导致的日志写入等问题。
- 修复 WebSocket 鉴权与连接状态处理中的异常问题。

### 变更

- **不兼容变更**：后端不再接受旧 `S_UID` / `S_TOKEN` / `S_TOKEN_TIME` Cookie，仅接受 `SESSIONID`。
- **不兼容变更**：移除 `/api/v1/user/refresh_token` 接口；会话过期后需重新登录。
- **不兼容变更**：移除 WebSocket 旧 query token 认证和 `auth.ws_legacy_token` 配置。
- **不兼容变更**：旧设备 token 认证不再使用，设备列表与下线统一走服务端会话。
- 移除旧 `session:` Redis 会话回退和 Redis 双写逻辑。
- 移除旧 `EncryptUserId` / `DecryptUserId` 与相关 Cookie 处理代码。
- `chen-sdk` 升级至 `1.7.0-rc1`，使用新版 `EncryptorUtil::SHA256Raw` 等能力。
- `prod.env` 新增 `ADMIN_PASSWD`、`AUTH_COOKIE_SECURE`、`WEBAUTHN_RP_ID`、`WEBAUTHN_RP_NAME`、`WEBAUTHN_ORIGIN`、
  `WEBAUTHN_USER_HANDLE_SECRET` 等配置。

### 升级注意

- 已有 `prod.env` 需要补齐新增的认证和 WebAuthn 占位符，否则环境切换脚本会失败。
- 升级后旧 Cookie 用户会变为未登录，需要重新登录；旧会话不会自动迁移。
- Passkey 依赖 HTTPS 或 `localhost`，且 `webauthn.rp_id` / `webauthn.origin` 必须与实际前端地址匹配。
- 调用旧 `refresh_token` 或使用 WebSocket URL token 的客户端需要同步升级。

## v1.0.0 (2026-10-18)

首个正式版本，提供完整的博客、组织协作与研发管理能力。

### 用户与安全

- 用户注册、登录、权限与账号管理
- 多设备登录、设备列表与远程下线
- 双因素认证（2FA）与恢复码
- GitHub OAuth 登录
- 密码找回、Token 刷新与账号安全设置

### 内容与互动

- 文章创建、编辑、审核、发布与定时发布
- 分类、标签、全文搜索与文章归档
- 点赞、收藏、评论与回复
- 通知、广播与未读消息

### 组织与协作

- 组织创建、加入、成员与角色管理
- GitHub 仓库绑定与 PR 状态同步
- 组织内作业分发与成员协作

### 作业与研发

- 作业创建、分发、提交与评分
- 需求、任务、Bug 管理
- 截止时间、状态流转与研发统计

### 资源与消息

- 文件与资源上传、分片上传
- 私信会话与消息记录
- 用户反馈、帮助中心与 FAQ

### 管理与运维

- 管理后台与数据统计
- 数据备份、导出与过期数据清理
- 日志、配置、定时任务与邮件通知
