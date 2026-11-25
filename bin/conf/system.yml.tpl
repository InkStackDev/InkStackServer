# InkStack 业务进程配置（由 switch-env.sh / scripts/cluster/render-instance.sh 渲染）
#
# 只保留当前代码实际读取的配置项：
#   - servers       HTTP / WebSocket 两种接入；业务只作为 RPC Hub 客户端，不再监听 rpc 端口
#   - fiber         协程栈大小
#   - workers       accept / io 工作线程
#   - logs          日志输出
#   - mysql         业务库
#   - fox_thread    异步 Redis 依赖的线程池
#   - redis         Redis 连接池（会话 / 缓存 / WS 路由 / 分布式租约）

servers:
  # HTTP API
  - address: ["0.0.0.0:${HTTP_PORT}"]
    keepalive: 1
    timeout: 1000
    name: chen-http/1.0.0
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: http
    negotiateH2: 1
    args:
      accept_qps: 1000      # 每秒最多接受的新连接数
      accept_burst: 2000    # 瞬时突发上限（缺省 = accept_qps）
      max_conn: 10000       # 最大并发连接数

  # WebSocket 网关（/ws/v1/session）
  - address: ["0.0.0.0:${WS_PORT}"]
    keepalive: 1
    timeout: 300000
    name: chen-ws/1.0.0
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: ws

fiber:
  stack_size: 65536

workers:
  io:
    thread_num: 4
  accept:
    thread_num: 1

logs:
  - name: root
    level: ${LOG_LEVEL}
    appenders:
      - type: FileLogAppender
        file: ${LOG_DIR}/root.log
        time_rotate: day
        retention_days: 14
      - type: StdoutLogAppender
  - name: system
    level: info
    appenders:
      - type: FileLogAppender
        file: ${LOG_DIR}/system.log
        time_rotate: day
        retention_days: 14
      - type: StdoutLogAppender
  - name: access
    level: info
    appenders:
      - type: FileLogAppender
        file: ${LOG_DIR}/access.log
        time_rotate: day
        retention_days: 14
      - type: StdoutLogAppender

mysql:
  dbs:
    techhaven:
      dbname: ${MYSQL_DBNAME}
      host: ${MYSQL_HOST}
      port: ${MYSQL_PORT}
      user: ${MYSQL_USER}
      passwd: ${MYSQL_PASSWD}
      pool: 10              # 单实例连接数；多实例部署时按 实例数 × pool 估算 max_connections

# 异步 Redis 依赖的线程池（名字固定为 redis，只读取 num / advance）
fox_thread:
  redis:
    num: 1
    advance: 0

redis:
  name: ${REDIS_DBNAME}
  config:
    ${REDIS_DBNAME}:
      host: ${REDIS_HOST}
      type: fox_redis
      pool: 4
      timeout: 1000
