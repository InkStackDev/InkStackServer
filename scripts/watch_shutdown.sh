#!/usr/bin/env bash
#
# 抓取 InkStackServer 关闭卡死现场。
#
# 背景：SDK 的 graceful shutdown 看门狗只有 "worker_shutdown_timeout_ms"（默认 6000ms）预算，
# 超时后打一行 "graceful shutdown exceeded ..." 就 _exit，不打线程栈。偶发卡死时只能靠外部采样定位。
#
# 本脚本在后台盯着 worker 日志，一旦出现 "stopping WorkerMgr"（关闭链路进入 SDK 的
# WorkerManager::stop 之前最后一条日志），就连续采样 worker 进程的所有线程栈。
#
# 用法：
#   scripts/watch_shutdown.sh                    # 默认盯 logs/system_<今天>.log
#   scripts/watch_shutdown.sh --log logs/system_2026-11-28.log
#   scripts/watch_shutdown.sh --duration 12 --interval 0.3
#
# 然后在另一个终端重启服务：bin/server -s。采样结束后脚本会打印输出文件路径。
#
# 采样优先级：gstack（gdb 的线程栈封装）> gdb 批处理 > /proc/<pid>/task/*/wchan 退化采样。
# gdb 需要与服务同用户（ptrace_scope=1 时同用户即可），否则只能拿到 wchan。

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_FILE="$ROOT_DIR/logs/system_$(date +%F).log"
PATTERN="stopping WorkerMgr"
DURATION=8
INTERVAL=0.5
SERVER_MATCH="bin/server"
WORKER_PID=""

usage()
{
    cat <<'EOF'
抓取 InkStackServer 关闭卡死现场。

用法：
  scripts/watch_shutdown.sh                     默认盯 logs/system_<今天>.log
  scripts/watch_shutdown.sh --log <file>        指定日志文件
  scripts/watch_shutdown.sh --duration 12       采样总时长（默认 8 秒）
  scripts/watch_shutdown.sh --interval 0.3      采样间隔（默认 0.5 秒）
  scripts/watch_shutdown.sh --pattern <text>    自定义触发标记（默认 "stopping WorkerMgr"）
  scripts/watch_shutdown.sh --pid <pid>         直接指定 worker pid，不再按进程名查找

用法示例：终端 A 跑本脚本，终端 B 执行 bin/server -s 触发优雅关闭。
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --log)
            LOG_FILE="$2"
            shift 2
            ;;
        --pattern)
            PATTERN="$2"
            shift 2
            ;;
        --duration)
            DURATION="$2"
            shift 2
            ;;
        --interval)
            INTERVAL="$2"
            shift 2
            ;;
        --pid)
            WORKER_PID="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "unknown option: $1" >&2
            usage
            ;;
    esac
done

# 优先读 /proc（不依赖 ps，部分受限环境里 ps 不可用），拿不到再退回 ps
parent_of()
{
    if [[ -r "/proc/$1/status" ]]; then
        awk '/^PPid:/ {print $2}' "/proc/$1/status"
        return
    fi
    ps -o ppid= -p "$1" 2>/dev/null | tr -d ' '
}

is_alive()
{
    if [[ -d /proc ]]; then
        [[ -d "/proc/$1" ]]
        return
    fi
    ps -p "$1" >/dev/null 2>&1
}

# master 只负责 fork/回收 worker，不加载业务模块，所以采样对象是它的子进程（worker）。
# 进程名匹配可能同时命中 master 与 worker，优先取「父进程也在匹配列表里」的那个。
find_worker()
{
    if [[ -n "$WORKER_PID" ]]; then
        echo "$WORKER_PID"
        return 0
    fi

    local -a pids=()
    local line
    while IFS= read -r line; do
        [[ -n "$line" ]] && pids+=("$line")
    done < <(pgrep -f "$SERVER_MATCH" 2>/dev/null)

    if ((${#pids[@]} == 0)); then
        return 1
    fi

    local pid ppid other
    for pid in "${pids[@]}"; do
        ppid="$(parent_of "$pid")"
        for other in "${pids[@]}"; do
            if [[ "$other" == "$ppid" ]]; then
                echo "$pid"
                return 0
            fi
        done
    done

    echo "${pids[0]}"
}

dump_stacks()
{
    local pid="$1"
    local out="$2"

    if command -v gstack >/dev/null 2>&1; then
        gstack "$pid" >>"$out" 2>&1
        return
    fi

    if command -v gdb >/dev/null 2>&1; then
        gdb -p "$pid" -batch -ex "set pagination off" -ex "thread apply all bt" >>"$out" 2>&1
        return
    fi

    # 没有 gdb：只采内核等待点。能区分 futex（等锁/join）与 epoll（等 IO），但看不到用户态调用栈
    local task
    for task in /proc/"$pid"/task/*; do
        [[ -d "$task" ]] || continue
        echo "thread ${task##*/} ($(cat "$task/comm" 2>/dev/null)) wchan=$(cat "$task/wchan" 2>/dev/null)" >>"$out"
    done
}

echo "watching ${LOG_FILE}"
echo "pattern : ${PATTERN}"
echo "duration: ${DURATION}s (interval ${INTERVAL}s)"
if ! pgrep -f "$SERVER_MATCH" >/dev/null 2>&1; then
    echo "注意：当前没有匹配 '${SERVER_MATCH}' 的进程，脚本会等你启动服务后再抓"
fi

echo "waiting for shutdown ... (Ctrl-C to abort)"

# 只认脚本启动之后新写入的日志：文件里已有的历史 "stopping WorkerMgr" 不能触发采样
baseline=0
if [[ -f "$LOG_FILE" ]]; then
    baseline="$(wc -c <"$LOG_FILE" | tr -d ' ')"
fi

# 每 0.2s 查一次日志新增部分，避免 tail -F 的进程管理
while :; do
    if [[ -f "$LOG_FILE" ]]; then
        size="$(wc -c <"$LOG_FILE" | tr -d ' ')"
        if ((size < baseline)); then
            baseline=0  # 日志被轮转/截断，从头看
        fi
        if ((size > baseline)); then
            hits="$(tail -c "+$((baseline + 1))" "$LOG_FILE" | grep -c -- "$PATTERN" || true)"
            if ((hits > 0)); then
                break
            fi
            # 长时间没触发时只保留尾部窗口，避免一直读整个文件
            if ((size - baseline > 8 * 1024 * 1024)); then
                baseline=$((size - 8 * 1024 * 1024))
            fi
        fi
    fi
    sleep 0.2
done

worker=""
deadline=$((SECONDS + 5))
while :; do
    worker="$(find_worker)"
    if [[ -n "$worker" ]]; then
        break
    fi
    if ((SECONDS >= deadline)); then
        echo "worker 已退出：这次关闭没有卡住，无需采样（找不到进程时也可以用 --pid 指定）"
        exit 0
    fi
    sleep 0.2
done

out="/tmp/shutdown_bt_${worker}_$(date +%Y%m%d_%H%M%S).txt"
: >"$out"
echo "# marker '${PATTERN}' seen at $(date '+%F %T')" >>"$out"

end=$((SECONDS + DURATION))
round=0
while ((SECONDS < end)); do
    round=$((round + 1))
    echo "===== sample ${round} $(date '+%H:%M:%S') =====" >>"$out"
    if ! is_alive "$worker"; then
        echo "worker ${worker} exited before sample ${round}" >>"$out"
        break
    fi
    dump_stacks "$worker" "$out"
    sleep "$INTERVAL"
done

echo "saved: ${out}"

if grep -q "wchan=" "$out"; then
    echo
    echo "wchan 分布（futex 多 = 卡在 join/等锁；epoll 多 = 卡在 IO 等待）："
    grep -o "wchan=[^ ]*" "$out" | sort | uniq -c | sort -rn | head -n 10
fi

echo
echo "下一步：把 ${out} 发回来。有 gstack 的输出看停在 Thread::join / futex_wait 的线程；"
echo "只有 wchan 的退化采样看各线程是否都停在 futex。同时对照日志里最后一条 \"shutdown: <step> begin\"。"
