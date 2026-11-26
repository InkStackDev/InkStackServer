# InkStack 多实例部署包说明

本包在打包时就已经为每个实例生成好 `conf/`，解压后直接启动即可。
进程管理使用框架自带的 `-d`（守护进程）与 `-s reload|stop`，不依赖 systemd 等系统服务配置。

## 1. 包内容

```
server                      # 业务进程（所有实例共享）
module/libblog.so
lib/libchen.so*             # 实体 libchen.so.1.6.0 + 链接 libchen.so.1、libchen.so
config/                     # 多语言配置（errors + messages）
conf/                       # 配置模板 + env 示例（新增实例 / 改配置后重新渲染用）
scripts/render-instance.sh  # 追加实例或改配置后重新渲染
scripts/clusterctl.sh       # 启停 / 状态 / 健康检查
server_1/{conf/system.yml, conf/TechHaven.yml, instance.env}
server_2/{...}
server_3/{...}
nginx/techhaven.conf.example
docs/multi-instance.md
VERSION
```

每个实例的 `conf/` 中是**已经渲染好的最终配置**：

| 实例     | 进程标题                             | HTTP | WS   | pid 文件              | 日志目录                   |
|----------|--------------------------------------|------|------|-----------------------|----------------------------|
| server_1 | `techhaven-server_1: master process` | 8090 | 8091 | `server_1/server.pid` | `/data/apps/server_1/logs` |
| server_2 | `techhaven-server_2: master process` | 8190 | 8191 | `server_2/server.pid` | `/data/apps/server_2/logs` |
| server_3 | `techhaven-server_3: master process` | 8290 | 8291 | `server_3/server.pid` | `/data/apps/server_3/logs` |

所有实例共享部署根目录下的 `config/`，以及 `share/data`（备份、导出）。

> **目录约定**：程序/配置目录由打包时的 `--deploy-path` 决定（默认 `/opt/techhaven`），实例目录直接放在该目录下
> （如 `/opt/techhaven/server_1`）；
> **只有日志**单独放在 `--log-root`（默认 `/data/apps`），即 `/data/apps/<name>/logs`。
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
./scripts/clusterctl.sh reload server_1         # 热重载单个实例
./scripts/clusterctl.sh rolling-restart all     # 逐个停→起，失败即停
./scripts/clusterctl.sh stop server_2           # 只停 server_2
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
./scripts/render-instance.sh --name server_4 --instance-id 4 \
    --deploy-path /opt/techhaven --env prod
./scripts/clusterctl.sh start server_4
```

也可以修改 `conf/*.yml.tpl` 后，对所有实例重新渲染再滚动重启。

## 5. 升级

```bash
tar xzf techhaven-cluster-<new>.tar.gz -C /tmp/new
cp /tmp/new/module/libblog.so /opt/techhaven/module/
rm -rf /opt/techhaven/config && cp -R /tmp/new/config /opt/techhaven/config
# 若新包带了实例配置变化（实例目录直接位于部署根目录）：
for d in /tmp/new/server_*/; do rsync -a "$d" "/opt/techhaven/$(basename "$d")/"; done
./scripts/clusterctl.sh rolling-restart all
```

## 6. 部署前检查

- [ ] MySQL / Redis 可用；MySQL `max_connections` 已按 `实例数 × pool` 调整
- [ ] Redis 已做主从/Sentinel 或 Cluster（会话、缓存、WS 路由、租约都依赖它）
- [ ] `share/data` 是共享存储（多机部署时），任意实例都能读到备份/导出文件
- [ ] 每个实例的 `instance-id` / `RPC_BUS_ID` 唯一（打包脚本已按 1..N 生成）
- [ ] Nginx（或现有网关）已配置 HTTP 与 WebSocket 的 `Upgrade` 头
- [ ] 首次部署的 `ADMIN_PASSWD` 已在打包前的 env 中设置（已存在管理员不会被覆盖）
