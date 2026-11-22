# 业务配置

# system default ai model for article summary
ai:
  type: glm
  model: glm-4.7-flash
  api_key: # system default AI API key, used when user has not configured their own AI settings
  timeout_ms: 60000
  max_attempts: 3

# 默认超级管理员：服务启动时若该邮箱/账号不存在则创建（已存在不会被覆盖）。
# 必须显式设置 ADMIN_PASSWD（>=6 位）；留空则跳过引导，首次部署需自行建号。
admin:
  account: admin
  passwd: ${ADMIN_PASSWD}
  email: admin@example.com

# 搜索服务（InkStackSearchServer）客户端配置
search:
  enabled: true
  hub_address: ${SEARCH_HUB_ADDRESS}
  timeout_ms: 1500
  update_timeout_ms: 5000
  connect_timeout_ms: 1000
  reconnect_interval_ms: 5000
  flush_interval_ms: 1000
  stats_interval_ms: 30000
  flush_batch_size: 200
  snapshot_page_size: 200
  snapshot_batch_bytes: 524288
  snapshot_rpc_timeout_ms: 10000
  dirty_key: search:dirty

# GitHub API 客户端配置（组织仓库/PR 同步）
github:
  api_base: https://api.github.com
  oauth_base: https://github.com
  timeout_ms: 15000
  detail_timeout_ms: 15000
  max_attempts: 3
  backoff_ms: 300
  user_agent: InkStackServer/1.0
  per_page: 50

# 第三方登录配置
oauth:
  github:
    client_id: ${GITHUB_CLIENT_ID}
    client_secret: ${GITHUB_CLIENT_SECRET}
    # 必须与 GitHub OAuth App 的 Authorization callback URL 完全一致（经前端同源代理）
    callback_url: ${GITHUB_CALLBACK_URL}
    # 授权成功后回跳的前端站点根地址
    frontend_base: ${FRONTEND_BASE}

# 会话与认证
auth:
  # 生产环境必须为 true；本地 HTTP 调试可设为 false
  cookie_secure: ${AUTH_COOKIE_SECURE}
  cookie_same_site: Lax
  # 会话绝对有效期（秒），<=0 表示不限制
  session_absolute_ttl: 604800

# WebAuthn / Passkey
webauthn:
  # 注册域（不含端口），例如 example.com；开发环境为 localhost
  rp_id: ${WEBAUTHN_RP_ID}
  rp_name: ${WEBAUTHN_RP_NAME}
  # 前端页面 origin（协议 + 域名 + 端口），必须与浏览器上报完全一致
  origin: ${WEBAUTHN_ORIGIN}
  timeout_ms: 60000
  user_verification: preferred   # preferred | required
  attestation: none
  challenge_ttl: 300
  # 用于生成稳定的 user.id，生产环境请配置随机密钥
  user_handle_secret: ${WEBAUTHN_USER_HANDLE_SECRET}

# Server configuration：运行目录、数据目录、pid 文件与实例编号
server:
  work_path: ${WORK_PATH}
  # 备份/导出/上传等文件根目录；多实例同机时建议指向共享目录
  data_path: ${DATA_PATH}
  # pid 文件落在 work_path 下，多实例各自独立
  pid_file: ${PID_FILE}
  # 进程标题前缀（ps/top 可读）；多实例建议 techhaven-a / techhaven-b
  process_name: ${PROCESS_NAME}
  # 部署实例编号（日志 / 指标 / hub 总线用）；与 rpc.bus_id 第 4 段保持一致
  instance_id: ${INSTANCE_ID}
  # 优雅退出：停止 accept 后等待在途协程退出的时间
  drain_timeout_ms: 3000
  # 内部 housekeeping 定时器间隔
  housekeeping_interval_ms: 500
  # master 等待 worker ready 的超时，以及 worker 崩溃后的重启间隔
  worker_ready_timeout_ms: 30000
  worker_restart_interval_ms: 2000

# RPC Hub 客户端配置：App 主动连接 hub，当前 hub 由 InkStackSearchServer 提供（默认 9010）
rpc:
  enabled: true
  hub_address: ${RPC_HUB_ADDRESS}
  # bus_id = <zone>.<group>.<service>.<instance>，每段 1..255
  #   peer_id = zone<<24 | group<<16 | service<<8 | instance（hub 全局唯一）
  #   func_id = group<<8 | service（同一服务所有实例共享）
  # 服务段参考：1=Search，2=App，3=Bot
  bus_id: ${RPC_BUS_ID}
  # 请求超时 / 连接超时 / 重连间隔 / 心跳间隔（毫秒）
  timeout_ms: 3000
  connect_timeout_ms: 1000
  reconnect_interval_ms: 5000
  heartbeat_interval_ms: 30000

# Redis object cache. Set enabled=false to bypass all entity caches and fall back to DB.
cache:
  obj:
    enabled: true