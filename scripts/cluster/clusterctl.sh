#!/usr/bin/env bash
#
# clusterctl.sh — 多实例启停 / 状态 / 健康检查（基于 server -d / -s，不依赖 systemd）
#
# 用法（在部署目录根下执行）:
#   ./scripts/clusterctl.sh start   <name>|all
#   ./scripts/clusterctl.sh stop    <name>|all
#   ./scripts/clusterctl.sh restart <name>|all
#   ./scripts/clusterctl.sh reload  <name>|all
#   ./scripts/clusterctl.sh status  [name|all]
#   ./scripts/clusterctl.sh health  [name|all]
#   ./scripts/clusterctl.sh rolling-restart all
#
# 说明:
#   - 每个实例目录结构: <name>/{conf/,server.pid,instance.env}（直接位于部署根目录）
#   - 所有命令都带同一个 -c，保证 pid 文件与配置对应
#   - 健康检查: GET /api/v1/site/status，要求 errno=0；无 curl 时退化为 TCP 探测
#
# 可用环境变量:
#   SERVER_BIN        server 二进制路径（默认 <root>/server）
#   START_TIMEOUT     启动等待秒数（默认 30）
#   STOP_TIMEOUT      停止等待秒数（默认 30）
#   HEALTH_TIMEOUT    健康检查等待秒数（默认 20）
#
set -euo pipefail

ROOT="$(cd "$(pwd)" && pwd)"
SERVER="${SERVER_BIN:-$ROOT/server}"

# libchen.so 与 server 同包，位于 <root>/lib；-d 守护进程会继承这里的环境变量
export LD_LIBRARY_PATH="$ROOT/lib:${LD_LIBRARY_PATH:-}"
START_TIMEOUT="${START_TIMEOUT:-30}"
STOP_TIMEOUT="${STOP_TIMEOUT:-30}"
HEALTH_TIMEOUT="${HEALTH_TIMEOUT:-20}"

usage() { sed -n '2,22p' "$0"; }
log() { echo "[cluster] $*"; }
die() { echo "[cluster][ERROR] $*" >&2; exit 1; }

[ -x "$SERVER" ] || die "找不到可执行文件: $SERVER"
[ -f "$ROOT/lib/libchen.so" ] || die "找不到共享库: $ROOT/lib/libchen.so"

# 实例目录直接位于部署根目录，用 instance.env 识别，避免把 conf/module/lib 等当成实例
list_instances() {
    local dir
    for dir in "$ROOT"/*/; do
        [ -f "$dir/instance.env" ] || continue
        basename "$dir"
    done | sort
}

instance_dir() {
    local name="$1"
    local dir="$ROOT/$name"
    [ -d "$dir" ] || die "实例不存在: $name"
    [ -f "$dir/conf/system.yml" ] || die "实例 $name 缺少配置: $dir/conf/system.yml"
    printf '%s' "$dir"
}

conf_dir() { printf '%s/conf' "$(instance_dir "$1")"; }

# pid 文件路径取自 instance.env（相对部署根目录），默认 <instance>/server.pid
pid_file() {
    local name="$1" rel
    rel="$(instance_env_field "$name" PID_FILE)"
    if [ -n "$rel" ]; then
        case "$rel" in
            /*) printf '%s' "$rel" ;;
            *)  printf '%s/%s' "$ROOT" "$rel" ;;
        esac
    else
        printf '%s/server.pid' "$(instance_dir "$name")"
    fi
}

instance_env_field() {
    local name="$1" key="$2"
    sed -n "s/^${key}=//p" "$(instance_dir "$name")/instance.env" 2>/dev/null | head -n1
}

pid_of() {
    local pf; pf="$(pid_file "$1")"
    [ -f "$pf" ] || return 1
    tr -dc '0-9' < "$pf"
}

pid_alive() {
    local pid
    pid="$(pid_of "$1" 2>/dev/null || true)"
    [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

tcp_alive() {
    local port="$1"
    (exec 3<>"/dev/tcp/127.0.0.1/$port") >/dev/null 2>&1
}

http_ok() {
    local port="$1" body
    if command -v curl >/dev/null 2>&1; then
        body="$(curl -fsS --max-time 3 "http://127.0.0.1:$port/api/v1/site/status" 2>/dev/null)" || return 1
        printf '%s' "$body" | grep -q '"errno"[[:space:]]*:[[:space:]]*0'
    else
        tcp_alive "$port"
    fi
}

wait_health() {
    local name="$1" port deadline
    port="$(instance_env_field "$name" HTTP_PORT)"
    [ -n "$port" ] || return 1
    deadline=$(( $(date +%s) + HEALTH_TIMEOUT ))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        http_ok "$port" && return 0
        sleep 1
    done
    return 1
}

start_one() {
    local name="$1" dir
    dir="$(instance_dir "$name")"
    if pid_alive "$name"; then
        log "$name 已在运行 (pid=$(pid_of "$name"))"
        return 0
    fi
    mkdir -p "$(instance_env_field "$name" LOG_DIR)" 2>/dev/null || true
    local data_dir; data_dir="$(instance_env_field "$name" DATA_PATH)"
    [ -n "$data_dir" ] && mkdir -p "$data_dir"
    log "启动 $name (conf=$dir/conf)"
    ( cd "$ROOT" && "$SERVER" -c "$dir/conf" -d ) || die "$name 启动命令失败"

    local deadline=$(( $(date +%s) + START_TIMEOUT ))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        pid_alive "$name" && break
        sleep 1
    done
    pid_alive "$name" || die "$name 在 ${START_TIMEOUT}s 内未写出 pid 文件"

    if ! wait_health "$name"; then
        die "$name 已启动但健康检查失败（http_port=$(instance_env_field "$name" HTTP_PORT)）"
    fi
    log "$name 启动完成 (pid=$(pid_of "$name"), http_port=$(instance_env_field "$name" HTTP_PORT))"
}

stop_one() {
    local name="$1" dir
    dir="$(instance_dir "$name")"
    if ! pid_alive "$name"; then
        log "$name 未运行"
        rm -f "$(pid_file "$name")"
        return 0
    fi
    local pid
    pid="$(pid_of "$name")"
    log "停止 $name (pid=$pid)"
    ( cd "$ROOT" && "$SERVER" -c "$dir/conf" -s stop ) >/dev/null 2>&1 || true

    local deadline=$(( $(date +%s) + STOP_TIMEOUT ))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        pid_alive "$name" || { log "$name 已停止"; return 0; }
        sleep 1
    done

    log "$name 优雅停止超时，发送 SIGTERM"
    kill -TERM "$pid" 2>/dev/null || true
    deadline=$(( $(date +%s) + 10 ))
    while [ "$(date +%s)" -lt "$deadline" ]; do
        pid_alive "$name" || { log "$name 已停止"; return 0; }
        sleep 1
    done

    log "$name 仍在运行，发送 SIGKILL"
    kill -KILL "$pid" 2>/dev/null || true
    rm -f "$(pid_file "$name")"
    pid_alive "$name" && die "$name 无法停止"
    log "$name 已强制停止"
}

reload_one() {
    local name="$1" dir
    dir="$(instance_dir "$name")"
    pid_alive "$name" || die "$name 未运行，无法 reload"
    log "热重载 $name"
    ( cd "$ROOT" && "$SERVER" -c "$dir/conf" -s reload ) || die "$name reload 失败"
    wait_health "$name" || die "$name reload 后健康检查失败"
    log "$name 热重载完成"
}

status_one() {
    local name="$1" state pid
    if pid_alive "$name"; then
        pid="$(pid_of "$name")"; state="running"
    else
        pid="-"; state="stopped"
    fi
    printf '%-10s %-8s pid=%-8s http=%-6s ws=%-6s bus=%s\n' \
        "$name" "$state" "$pid" \
        "$(instance_env_field "$name" HTTP_PORT)" \
        "$(instance_env_field "$name" WS_PORT)" \
        "$(instance_env_field "$name" RPC_BUS_ID)"
}

health_one() {
    local name="$1" port
    port="$(instance_env_field "$name" HTTP_PORT)"
    if wait_health "$name"; then
        log "$name 健康 (http_port=$port)"
        return 0
    fi
    echo "[cluster][ERROR] $name 健康检查失败 (http_port=$port)" >&2
    return 1
}

resolve_targets() {
    local target="$1"
    if [ "$target" = "all" ]; then
        list_instances
    else
        printf '%s\n' "$target"
    fi
}

CMD="${1:-}"
TARGET="${2:-all}"

case "$CMD" in
    start)
        for n in $(resolve_targets "$TARGET"); do start_one "$n"; done
        ;;
    stop)
        for n in $(resolve_targets "$TARGET"); do stop_one "$n"; done
        ;;
    restart)
        for n in $(resolve_targets "$TARGET"); do stop_one "$n"; start_one "$n"; done
        ;;
    reload)
        for n in $(resolve_targets "$TARGET"); do reload_one "$n"; done
        ;;
    status)
        for n in $(resolve_targets "$TARGET"); do status_one "$n"; done
        ;;
    health)
        rc=0
        for n in $(resolve_targets "$TARGET"); do health_one "$n" || rc=1; done
        exit "$rc"
        ;;
    rolling-restart)
        [ "$TARGET" = "all" ] || die "rolling-restart 只支持 all"
        for n in $(list_instances); do
            stop_one "$n"
            start_one "$n"
        done
        log "滚动重启完成"
        ;;
    -h|--help|"")
        usage
        [ -z "$CMD" ] && exit 2 || exit 0
        ;;
    *)
        die "未知命令: $CMD"
        ;;
esac
