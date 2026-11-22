# InkStack 多实例部署包说明

本包在打包时就已经为每个实例生成好 `conf/`，解压后直接启动即可。
进程管理使用框架自带的 `-d`（守护进程）与 `-s reload|stop`，不依赖 systemd 等系统服务配置。

## 1. 包内容

```
server                      # 业务进程（所有实例共享）
module/libblog.so
lib/libchen.so
errors.json
locale/
conf/                       # 配置模板 + env 示例（新增实例 / 改配置后重新渲染用）
scripts/render-instance.sh  # 追加实例或改配置后重新渲染
scripts/clusterctl.sh       # 启停 / 状态 / 健康检查
instances/
├── a/{conf/system.yml, conf/TechHaven.yml, instance.env}
├── b/{...}
└── c/{...}
nginx/techhaven.conf.example
docs/multi-instance.md
VERSION
```

每个实例的 `conf/` 中是**已经渲染好的最终配置**：

| 实例 | 进程标题                      | HTTP | WS   | pid 文件                 | 日志目录                      |
|------|-------------------------------|------|------|--------------------------|-------------------------------|
| a    | `techhaven-a: master process` | 8090 | 8091 | `instances/a/server.pid` | `/data/apps/instances/a/logs` |
| b    | `techhaven-b: master process` | 8190 | 8191 | `instances/b/server.pid` | `/data/apps/instances/b/logs` |
| c    | `techhaven-c: master process` | 8290 | 8291 | `instances/c/server.pid` | `/data/apps/instances/c/logs` |

所有实例共享部署根目录下的 `errors.json` / `locale`，以及 `share/data`（备份、导出）。

> **目录约定**：程序/配置目录由打包时的 `--deploy-path` 决定（默认 `/opt/techhaven`）；
> **只有日志**单独放在 `--log-root`（默认 `/data/apps`），即 `/data/apps/instances/<name>/logs`。
> 如果解压到别的路径，需要重新打包，或在部署端用 `scripts/render-instance.sh` 重新生成实例配置。

## 2. 首次部署

```bash
mkdir -p /opt/techhaven
tar xzf techhaven-cluster-*.tar.gz -C /opt/techhaven
cd /opt/techhaven

./scripts/clusterctl.sh start all
./scripts/clusterctl.sh status
./scripts/clusterctl.sh health all
```

启动时会自动创建各实例的 `logs/` 与 `share/data/` 目录，并设置 `LD_LIBRARY_PATH=<部署目录>/lib`，
无需手动导出 `libchen.so` 路径。

## 3. 日常操作

```bash
./scripts/clusterctl.sh status                  # 所有实例状态
./scripts/clusterctl.sh health all              # GET /api/v1/site/status
./scripts/clusterctl.sh reload a                # 热重载单个实例
./scripts/clusterctl.sh rolling-restart all     # 逐个停→起，失败即停
./scripts/clusterctl.sh stop b                  # 只停 b
```

开机自启直接在你现有的启动入口里执行：

```bash
/opt/techhaven/scripts/clusterctl.sh start all
```

## 4. 追加实例 / 修改配置

`conf/prod.env.example` 是环境变量模板；实际连接信息已经写进各实例的 `conf/`。
需要新增实例或调整端口时：

```bash
cp conf/prod.env.example conf/prod.env   # 填 MySQL / Redis / hub / 密钥
./scripts/render-instance.sh --name d --instance-id 4 \
    --deploy-path /opt/techhaven --env prod
./scripts/clusterctl.sh start d
```

也可以修改 `conf/*.yml.tpl` 后，对所有实例重新渲染再滚动重启。

## 5. 升级

```bash
tar xzf techhaven-cluster-<new>.tar.gz -C /tmp/new
cp /tmp/new/module/libblog.so /opt/techhaven/module/
cp /tmp/new/errors.json /opt/techhaven/errors.json
rsync -a /tmp/new/locale/ /opt/techhaven/locale/
# 若新包带了实例配置变化：
rsync -a /tmp/new/instances/ /opt/techhaven/instances/
./scripts/clusterctl.sh rolling-restart all
```

## 6. 部署前检查

- [ ] MySQL / Redis 可用；MySQL `max_connections` 已按 `实例数 × pool` 调整
- [ ] Redis 已做主从/Sentinel 或 Cluster（会话、缓存、WS 路由、租约都依赖它）
- [ ] `share/data` 是共享存储（多机部署时），任意实例都能读到备份/导出文件
- [ ] 每个实例的 `instance-id` / `RPC_BUS_ID` 唯一（打包脚本已按 1..N 生成）
- [ ] Nginx（或现有网关）已配置 HTTP 与 WebSocket 的 `Upgrade` 头
- [ ] 首次部署的 `ADMIN_PASSWD` 已在打包前的 env 中设置（已存在管理员不会被覆盖）
