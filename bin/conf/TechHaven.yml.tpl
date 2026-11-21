# 业务配置

# system default ai model for article summary
ai:
  type: glm
  model: glm-4.7-flash
  api_key: # system default AI API key, used when user has not configured their own AI settings

# 默认超级管理员：服务启动时若该邮箱对应的管理员不存在则自动创建，用于首次登录并配置 SMTP 等系统设置。
# 出于安全考虑不提供默认密码：首次部署必须在 <ENV>.env 中显式设置 ADMIN_PASSWD，
# 已存在的管理员密码不会被启动流程覆盖。
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

# Server configuration, including work path, PID file, and email service settings for sending notifications or alerts.
server:
  work_path: ${WORK_PATH}
  data_path: ${DATA_PATH}
  pid_file: server.pid
  # 部署实例编号（日志 / 指标用）；与 rpc.bus_id 第 4 段保持一致
  instance_id: ${INSTANCE_ID}

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