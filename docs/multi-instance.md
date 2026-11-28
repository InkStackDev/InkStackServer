# 多实例部署说明

本文说明 InkStackServer 在“同一套 MySQL + Redis 之上跑多个实例（多台机器或多个容器）”时的前提、已具备能力与仍需人工保证的
部分。架构分层与职责见 [`architecture.md`](architecture.md)。

## 1. 已经具备的多实例能力

| 能力                | 实现位置                                  | 说明                                                                                |
|---------------------|-------------------------------------------|-------------------------------------------------------------------------------------|
| 会话共享            | `src/Auth/SessionStore.cc`                | 会话存 Redis `auth:sess:*`，Cookie 只保存随机 session id，任意实例都能校验          |
| 跨实例 WS 下行      | `src/Message/MessageRouter.cc`            | Redis presence（`ws:device:*` / `ws:user:*`）+ 每 worker 收件箱 + 心跳 + 死实例清理 |
| 跨实例踢人 / 广播   | `src/Message/MessageRouter.cc`            | `CloseDevice` / `CloseUser` / `Broadcast` 均通过收件箱投递                          |
| 实体缓存            | `src/Cache/EntityCache.hpp`               | 全部走 Redis `cache:obj:*`，不使用进程内 LRU，实例间天然一致                        |
| 列表 / 计数缓存     | `src/Manager/CacheUtil.cc`                | `cache:count:*` / `cache:list:*`，配合 Redis 版本号失效                             |
| 定时任务互斥        | `src/Job/RedisLease.cc`                   | `SET NX EX` + token 校验删除，默认任务全局只跑一份                                  |
| 浏览去重 / 点赞关系 | `src/Service/ArticleCounterService.cc`    | Redis `art:view:*`、`pra_*` / `fav_*` 哈希                                          |
| 系统设置 / 通知版本 | `src/Manager/SystemSettingsManager.cc` 等 | Redis 版本号做跨实例失效                                                            |
| 滚动发布            | `src/BlogModule.cc`                       | SIGQUIT → 停止 accept → drain → 关闭 WS，旧 worker 不会被硬断连                     |

## 2. 部署前必须保证

### 2.1 每个实例的标识唯一

```sh
INSTANCE_ID=1          # 每个实例不同
RPC_BUS_ID=1.1.2.1     # 第 4 段与 INSTANCE_ID 保持一致，每个实例不同
```

- WS worker id 使用 `instance:pid`，`INSTANCE_ID` 相同也不会互相顶掉在线状态；
- 但 RPC hub 的 `bus_id` 必须唯一，否则不同实例的 RPC 消息可能串线。

### 2.2 Redis 是强依赖

会话、缓存、WS 路由、分布式租约全部在 Redis 上。单点 Redis 故障等于全站登录失败，生产环境至少需要：

- 主从 + 自动故障转移（Sentinel），或
- Cluster（配置使用 `fox_redis_cluster`）。

`redis.config.<name>` 中的 `pool` / `timeout` 按实例并发量评估，`timeout` 建议不低于 500ms。

> `REDIS_DBNAME` 是**逻辑池名**（`GetRedisPoolName()` 的返回值），不是 Redis 的 DB 序号。多个环境共用一台 Redis 时，
> 必须用不同的池名指向不同的 Redis 实例，否则 key 会互相覆盖。

### 2.3 文件必须共享

以下路径必须是所有实例可见的共享存储（NFS / 云盘 / 对象存储挂载）：

| 内容     | 位置                                         |
|----------|----------------------------------------------|
| 上传资源 | `ResourceInfo.path`，相对 `server.work_path` |
| 备份文件 | `server.data_path`                           |
| 导出文件 | `server.data_path`                           |

否则会出现“实例 A 创建/上传的文件，在实例 B 下载 404、清理任务删不到”的问题。

> `BackupCreateServlet` 调用的是**本机** `mysqldump`。多实例部署时建议只让一个实例承担备份/导出职责（例如单独部署一个
> `backup` 实例），或改为共享存储 + 统一调度。

### 2.4 MySQL 连接数

每个实例各自持有 `mysql.dbs.<name>.pool` 条连接，`max_connections` 至少按：

```text
实例数 × 每实例 worker 数 × pool × 2（预留）
```

估算。

### 2.5 初始化数据

- 表结构需要在建库阶段准备好（`dbproxy/xml/` + `make orm` 生成的建表/迁移逻辑）；服务启动流程**不会**自动建表或迁移。
- 首个管理员由 `admin` 配置引导：`<ENV>.env` 里显式设置 `ADMIN_PASSWD`（>=6 位）即可在首次启动时创建，
  已存在（邮箱或账号命中）不会被覆盖；`ADMIN_PASSWD` 留空则跳过引导。
  多实例同时启动依赖 `user` 表唯一索引，重复插入按“已存在”处理。

### 2.6 打包与启动

```bash
# 构建机上（需要 bin/conf/prod.env 里已有真实连接信息）
make cluster ENV=prod INSTANCES=3
# 或：./scripts/cluster/package-cluster.sh prod --instances 3 --deploy-path /opt/techhaven
# 产物: techhaven-cluster-<version>-<arch>-<ts>.tar.gz (+ .sha256)
```

产物根目录下已经生成好 `server_1/`、`server_2/`、`server_3/`（实例目录直接位于部署根目录），
每个实例的 `conf/`、`instance.env`、端口、`instance_id`、`RPC_BUS_ID`、日志目录、pid 文件互相独立：

| 实例     | 进程标题                             | HTTP | WS   | pid                   | 日志                       |
|----------|--------------------------------------|------|------|-----------------------|----------------------------|
| server_1 | `techhaven-server_1: master process` | 8090 | 8091 | `server_1/server.pid` | `/data/apps/server_1/logs` |
| server_2 | `techhaven-server_2: master process` | 8190 | 8191 | `server_2/server.pid` | `/data/apps/server_2/logs` |
| server_3 | `techhaven-server_3: master process` | 8290 | 8291 | `server_3/server.pid` | `/data/apps/server_3/logs` |

```bash
# 部署机上
mkdir -p /opt/techhaven
tar xzf techhaven-cluster-*.tar.gz -C /opt/techhaven
cd /opt/techhaven
./scripts/clusterctl.sh start all
./scripts/clusterctl.sh health all
```

- 程序/配置目录来自打包时的 `--deploy-path`（默认 `/opt/techhaven`），实例目录直接放在该目录下；**只有日志**单独放到
  `--log-root`（默认 `/data/apps`），即 `/data/apps/<name>/logs`。换目录需重新打包，
  或部署后用 `scripts/render-instance.sh` 重新渲染。
- 新增实例或修改端口：`./scripts/render-instance.sh --name server_4 --instance-id 4 --deploy-path /opt/techhaven`。
- 进程管理用框架自带的 `-d` 与 `-s reload|stop`，`clusterctl.sh` 的所有命令都带同一个 `-c`，不会误停其它实例。
- 业务只作为 RPC Hub 客户端，不再监听 rpc 端口；`RPC_BUS_ID` 末段与 `--instance-id` 保持一致。

CI（GitHub Actions）：

- `Build & Package` 只产出集群包：`techhaven-cluster-<tag>-<arch>.tar.gz` + `.sha256`
  （`CLUSTER_INSTANCES=1` 即单实例）。打包参数全部来自仓库 Variables，触发时无需填写：
  `PKG_ENV`（默认 `prod`）、`CLUSTER_INSTANCES`、`DEPLOY_PATH`、`CLUSTER_LOG_ROOT`。
- `Deploy` 支持：
  - `cold`：首次全量解压并 `start all`
  - `hot`：替换 `module/`、`config/`、`scripts/` 后 `rolling-restart all`
  - `module`：**只替换 `module/*.so`**（旧文件备份为 `*.so.prev`），不重启任何实例，
    由用户登录服务器自行 `clusterctl.sh reload <name>`，适合灰度验证
  - `stop`：`stop all`

  灰度示例：

  ```bash
  cd /opt/techhaven
  ./scripts/clusterctl.sh reload server_1 && ./scripts/clusterctl.sh health server_1   # 先切一个实例
  ./scripts/clusterctl.sh rolling-restart all                                           # 确认后全量
  ```

  回滚：`cd module && for f in *.so.prev; do mv "$f" "${f%.prev}"; done`，再 reload。
- `Deploy` 使用的仓库变量 `DEPLOY_PATH` 必须与打包时的 `deploy_path` 一致，否则实例配置里的
  `work_path`/pid 路径与运行目录不匹配。
- 集群包里 `<name>/conf/*.yml` 是预渲染的，**包含真实密码与密钥**：仓库保持 private，
  artifact 保留期设短，Release 资产按敏感文件管理。

## 3. 定时任务与计数

### 3.1 租约任务（全局一份）

`JobRunner` 默认使用 `RedisLease`，同一时刻只有一个实例执行。Redis 不可用时**跳过本轮**（fail-closed），
并每 60s 打一条 WARN，恢复后打一条 INFO。

| 任务                                 | 说明               |
|--------------------------------------|--------------------|
| `article:publish`                    | 到期文章发布       |
| `org:repo_sync`                      | 组织仓库元数据同步 |
| `org:pr_sync`                        | 组织 PR 兜底同步   |
| `notify:broadcast_cleanup`           | 过期广播清理       |
| `search:snapshot`（SearchSync 内部） | 搜索全量重建       |

### 3.2 每实例任务（不使用租约）

`use_lease = false` 的任务每个 worker 都会执行，用来处理**进程内状态**：

| 任务            | 说明                                    |
|-----------------|-----------------------------------------|
| `article:flush` | 把本进程累计的浏览/点赞/收藏增量刷到 DB |

计数刷盘用的是增量 SQL：

```sql
UPDATE article SET views = views + ?, praise = praise + ?, favorites = favorites + ? WHERE id = ?
```

因此多实例各自的增量是叠加关系，不会出现“后写覆盖先写”。刷盘成功后会对相应文章执行 `Evict`，让对象缓存下次从 DB
重新加载，避免缓存值与 DB 长期漂移。

## 4. 观测

- 每个实例每 5s 把本进程 `Registry::RenderJson()` 快照写入 Redis：
  `metrics:inst:<instance_id>:<pid>`（TTL 60s），并用 zset `metrics:instances` 维护存活列表。
  写入前会裁掉零值的路由级指标（`route` 标签下 count/value 为 0），聚合侧缺失即视作 0。
- 路由指标以 `route` + `method` 两个标签区分，同一路径的 GET / POST / DELETE 各自独立统计。
- `/api/v1/admin/metrics` 汇总所有存活实例：counter / gauge / histogram 求和；`blog_info` 取实例编号最小的一份
  （PID 稳定）；`blog_online_users`、系统内存取最大；运行时长取最小。响应结构为
  `{ "metrics": [...], "helps": { name: help }, "instance_count": N, "degraded": bool }`：
  help 按指标名去重后放在顶层 `helps`，不再逐条内联。
- 路由 QPS 不再注册 gauge，由消费方用 `requests / blog_process_uptime_seconds`（或两次轮询的差值）自行计算。
- Redis 不可用时退化为只返回当前实例的指标，并置 `degraded=true`；前端会提示指标聚合不完整。
- Prometheus 侧仍按实例抓取；`blog_info` 的 `pid` 标签可用于区分实例。
- 跨实例在线人数来自 Redis `ws:online:users`，`blog_online_users` 在任意实例上都一致。
- 实例被 `kill -9` 后，其 presence 由其他实例通过 `zscore` 心跳判断并清理，约一个心跳周期内恢复；
  指标快照最多 60s 后从聚合中消失。

## 5. 上线验证清单

1. 在实例 A 登录，把后续请求打到实例 B，确认会话有效。
2. 在 A 建立 WS 连接，从 B 触发通知/私信，确认能收到。
3. 两个实例同时浏览/点赞同一篇文章，对比 DB 计数是否等于两边之和。
4. `kill -9` 一个实例，确认在线人数与 presence 被清理。
5. 滚动重启（SIGHUP / SIGTERM）期间无 5xx，旧 worker 正常 drain 退出。
6. 停掉 Redis，确认业务接口返回明确错误、后台任务不再重复执行；恢复 Redis 后任务自动继续。
7. `ss -lntp` 只看到 HTTP / WebSocket 端口（不再有 rpc 端口），启动日志为 `2 server type(s)`。

## 6. 已知限制

- 设备从实例 A 重连到实例 B 的短暂窗口内，A 上的旧连接可能仍会收到一份下行消息；客户端按消息 id 去重即可。
- WebSocket 连接本身仍绑定在接入的 worker 上，跨实例只传下行消息，不做连接迁移。
- `metrics` 与 `WSConnectionManager` 的计数都是进程内状态，不做跨实例汇总。

## 7. 关闭卡死（graceful shutdown exceeded）排查

现象：日志出现

```
[ERROR] [system] src/WorkerRuntime.cc:169 graceful shutdown exceeded 6000ms, forcing exit
        (possible deadlock in unload/scheduler stop)
```

### 7.1 先理解这条日志的含义

6000ms 是 `server.worker_shutdown_timeout_ms` 的**看门狗预算**，不是 `join` 的超时。SDK 的关闭链路
（`WorkerManager::stop()` → `TimerManager::cancelAllTimers()` → 每个 `Scheduler::stop()` → `Thread::join()`）里没有任何
超时参数，所以这条日志的含义是「已经卡死，6 秒后被强杀」，而不是「关得慢」。因此调大 `worker_shutdown_timeout_ms`
只是把强杀推迟，不解决问题；`server.drain_timeout_ms`（默认 3000）控制的是停止 accept 后等待在途请求的时间，与它无关。

### 7.2 业务侧是否已经关干净

`BlogModule::onDeactivate` / `onUnload` 每一步都有耗时日志：

```
shutdown: onDeactivate begin
shutdown: ArticleCounterService::FlushDirty done in 3ms
shutdown: WSMgr::closeAllConnections done in 12ms
shutdown: onDeactivate done
shutdown: onUnload begin
shutdown: JobRunner::StopAll done in 0ms
...
shutdown: onUnload done
```

- 出现 `shutdown: onUnload done` 后再超时：业务侧已经关干净（计数已落库、WS 已断），强杀只影响退出观感，
  数据一致性无影响；
- 最后一条是 `shutdown: <step> begin` 而没有对应的 `done`：卡在这一步，按步骤名定位；
- 一条 `shutdown:` 都没有、却出现 `stopping WorkerMgr...`（SDK 日志）：说明已经进入 SDK 的调度器关闭阶段。

### 7.3 采栈定位

```
# 终端 A
scripts/watch_shutdown.sh

# 终端 B
bin/server -s
```

脚本在日志出现 `stopping WorkerMgr` 后连续采样 worker 的全部线程栈，输出到
`/tmp/shutdown_bt_<pid>_<时间>.txt`。判定方式：

| 采样结果                                                         | 结论                                          |
|------------------------------------------------------------------|-----------------------------------------------|
| 只有一个线程停在 `Thread::join` / `futex_wait`，且回溯到业务函数 | 业务协程没退出（WS 关闭、长任务、FiberMutex） |
| 多个线程都在 join，被 join 的线程自己停在 `epoll_wait` / idle    | SDK 的调度器关闭问题，需要带栈上报            |
| 没有 gdb，退化采样里所有线程都停在 `futex`                       | 同上，进一步用 `gstack` 取用户态栈            |

服务与 gdb 需要同用户（`ptrace_scope=1` 时同用户即可）。机器上没有 gdb 时脚本会退化成读
`/proc/<pid>/task/*/wchan`，只能区分「等锁」还是「等 IO」。

脚本的两个行为要注意：

- 只认**脚本启动之后**新写入的日志；历史日志里已有的 `stopping WorkerMgr` 不会误触发（否则上一轮的关闭记录会立刻命中）。
  所以先起脚本、再触发重启即可，脚本可以先于服务启动；
- 关闭很快时 worker 已经退出，只会打印 `worker 已退出：这次关闭没有卡住，无需采样`——这正好可以当对照组用：
  同一台机器上跑 10 次，如果 10 次都是这句，说明卡死不是必然路径。

### 7.4 本项目里已知可能拖长关闭的任务

关闭时会先 `JobRunner::StopAll()`（只取消定时器，不等待在跑的任务），所以如果某个任务正在执行，线程要等它自己跑完：

- `org:repo_sync` / `org:pr_sync`：调 GitHub API（`OrganizationRepoService::SyncAll` /
  `OrganizationRepoPrService::SyncAll`），单次耗时没有上界；
- `article:daily_recommend`：候选池最多 300 篇，还要对 `buffer_size * 3` 篇查评论数（N+1）；
- `notify:broadcast_cleanup`。

如果采栈确认卡在这些任务里，优先缩短它们的单次耗时（批量取评论数、给外部请求加超时），而不是调大关闭预算。
