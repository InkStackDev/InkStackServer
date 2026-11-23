#!/usr/bin/env bash
#
# render-instance.sh — 为单个实例渲染一份独立配置
#
# 用法:
#   ./scripts/render-instance.sh --name server_1 --instance-id 1
#   ./scripts/render-instance.sh --root dist-cluster --deploy-path /opt/techhaven \
#       --name server_2 --instance-id 2 --http-port 8190 --ws-port 8191
#
# 参数:
#   --name NAME          实例名，对应 <root>/NAME/
#   --instance-id N      实例编号（server.instance_id / rpc.bus_id 末段）
#   --http-port PORT     默认 8090 + (N-1)*100
#   --ws-port PORT       默认 8091 + (N-1)*100
#   --env ENV            读取 <root>/conf/<ENV>.env，默认 prod
#   --env-file PATH      指定 env 文件（优先于 --env）
#   --root PATH          配置模板根目录，默认当前目录
#   --deploy-path PATH   部署根目录（程序/配置所在路径），默认 /opt/techhaven
#   --log-root PATH      日志根目录，默认 /data/apps，即 <log-root>/NAME/logs
#   --data-path PATH     备份/导出目录，默认 <deploy-path>/share/data
#   --bus-id ID          默认 1.1.2.<N>
#   --process-name NAME  进程标题前缀，默认 techhaven-<实例名>
#
# 产物:
#   <root>/NAME/conf/system.yml
#   <root>/NAME/conf/TechHaven.yml
#   <root>/NAME/instance.env
#
# 说明:
#   - server.work_path 指向部署根目录（errors.json / locale / uploads 共享）
#   - 每个实例的 pid 文件、日志、data 路径互不影响
#
set -euo pipefail

NAME=""
ARG_INSTANCE_ID=""
ARG_HTTP_PORT=""
ARG_WS_PORT=""
ENV_NAME="prod"
ENV_FILE=""
ARG_DATA_PATH=""
ARG_LOG_ROOT=""
ARG_PROCESS_NAME=""
ARG_BUS_ID=""
ARG_DEPLOY_PATH=""
ROOT="$(pwd)"

usage() { sed -n '2,31p' "$0"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --name)          NAME="${2:-}"; shift 2 ;;
        --name=*)        NAME="${1#*=}"; shift ;;
        --instance-id)   ARG_INSTANCE_ID="${2:-}"; shift 2 ;;
        --instance-id=*) ARG_INSTANCE_ID="${1#*=}"; shift ;;
        --http-port)     ARG_HTTP_PORT="${2:-}"; shift 2 ;;
        --http-port=*)   ARG_HTTP_PORT="${1#*=}"; shift ;;
        --ws-port)       ARG_WS_PORT="${2:-}"; shift 2 ;;
        --ws-port=*)     ARG_WS_PORT="${1#*=}"; shift ;;
        --env)           ENV_NAME="${2:-}"; shift 2 ;;
        --env=*)         ENV_NAME="${1#*=}"; shift ;;
        --env-file)      ENV_FILE="${2:-}"; shift 2 ;;
        --env-file=*)    ENV_FILE="${1#*=}"; shift ;;
        --root)          ROOT="${2:-}"; shift 2 ;;
        --root=*)        ROOT="${1#*=}"; shift ;;
        --deploy-path)   ARG_DEPLOY_PATH="${2:-}"; shift 2 ;;
        --deploy-path=*) ARG_DEPLOY_PATH="${1#*=}"; shift ;;
        --log-root)      ARG_LOG_ROOT="${2:-}"; shift 2 ;;
        --log-root=*)    ARG_LOG_ROOT="${1#*=}"; shift ;;
        --data-path)     ARG_DATA_PATH="${2:-}"; shift 2 ;;
        --data-path=*)   ARG_DATA_PATH="${1#*=}"; shift ;;
        --process-name)  ARG_PROCESS_NAME="${2:-}"; shift 2 ;;
        --process-name=*) ARG_PROCESS_NAME="${1#*=}"; shift ;;
        --bus-id)        ARG_BUS_ID="${2:-}"; shift 2 ;;
        --bus-id=*)      ARG_BUS_ID="${1#*=}"; shift ;;
        -h|--help)       usage; exit 0 ;;
        *) echo "未知参数: $1" >&2; usage >&2; exit 2 ;;
    esac
done

ROOT="$(cd "$ROOT" && pwd)"
[ -n "$NAME" ] || { echo "错误: 缺少 --name" >&2; exit 2; }
case "$NAME" in
    *[!A-Za-z0-9_-]*) echo "错误: 非法 --name '$NAME'（只允许字母/数字/_/-）" >&2; exit 2 ;;
    conf|module|lib|scripts|nginx|docs|locale|share|server|errors.json|VERSION)
        echo "错误: --name '$NAME' 与部署根目录的既有文件/目录冲突" >&2; exit 2 ;;
esac
[ -n "$ARG_INSTANCE_ID" ] || { echo "错误: 缺少 --instance-id" >&2; exit 2; }
case "$ARG_INSTANCE_ID" in
    ''|*[!0-9]*) echo "错误: --instance-id 必须是正整数" >&2; exit 2 ;;
esac

[ -f "$ROOT/conf/system.yml.tpl" ] || {
    echo "错误: $ROOT/conf/system.yml.tpl 不存在，请在部署根目录执行或指定 --root" >&2
    exit 2
}

if [ -z "$ENV_FILE" ]; then
    ENV_FILE="$ROOT/conf/$ENV_NAME.env"
fi
[ -f "$ENV_FILE" ] || { echo "错误: 找不到 env 文件 $ENV_FILE" >&2; exit 2; }

# 部署根目录：可以是打包时的目标路径，与模板根目录分离
if [ -n "$ARG_DEPLOY_PATH" ]; then
    DEPLOY_PATH="$ARG_DEPLOY_PATH"
else
    DEPLOY_PATH="/opt/techhaven"
fi
LOG_ROOT="${ARG_LOG_ROOT:-/data/apps}"

# 1. 先读 env 文件（公共配置）
set -a
# shellcheck disable=SC1090
. "$ENV_FILE"
set +a

# 2. 再套用实例级默认值/覆盖值
BASE=$(( (ARG_INSTANCE_ID - 1) * 100 ))
HTTP_PORT="${ARG_HTTP_PORT:-$((8090 + BASE))}"
WS_PORT="${ARG_WS_PORT:-$((8091 + BASE))}"
DATA_PATH="${ARG_DATA_PATH:-${DATA_PATH:-$DEPLOY_PATH/share/data}}"
BUS_ID="${ARG_BUS_ID:-1.1.2.$ARG_INSTANCE_ID}"
PROCESS_NAME="${ARG_PROCESS_NAME:-techhaven-$NAME}"

INSTANCE_DIR="$DEPLOY_PATH/$NAME"
CONF_DIR="$ROOT/$NAME/conf"

export WORK_PATH="$DEPLOY_PATH"
export LOG_DIR="$LOG_ROOT/$NAME/logs"
export PID_FILE="$NAME/server.pid"
export DATA_PATH
export INSTANCE_ID="$ARG_INSTANCE_ID"
export RPC_BUS_ID="$BUS_ID"
export PROCESS_NAME
export HTTP_PORT
export WS_PORT

mkdir -p "$CONF_DIR"

# 优先 envsubst，缺失时用 awk 按环境变量替换（支持空格/特殊字符）
render_one() {
    local tpl="$1" out="$2"
    if command -v envsubst >/dev/null 2>&1; then
        envsubst < "$tpl" > "$out"
    else
        awk '{
            while (match($0, /\$\{[A-Za-z_][A-Za-z0-9_]*\}/)) {
                name = substr($0, RSTART + 2, RLENGTH - 3)
                $0 = substr($0, 1, RSTART - 1) ENVIRON[name] substr($0, RSTART + RLENGTH)
            }
            print
        }' "$tpl" > "$out"
    fi

    if grep -qE '\$\{[A-Za-z_][A-Za-z0-9_]*\}' "$out"; then
        echo "错误: $out 中存在未替换的占位符，请检查 $ENV_FILE 是否缺变量" >&2
        exit 1
    fi
}

for tpl in "$ROOT"/conf/*.yml.tpl; do
    [ -f "$tpl" ] || continue
    render_one "$tpl" "$CONF_DIR/$(basename "${tpl%.tpl}")"
done

cat > "$ROOT/$NAME/instance.env" <<EOF
NAME=$NAME
PROCESS_NAME=$PROCESS_NAME
INSTANCE_ID=$INSTANCE_ID
HTTP_PORT=$HTTP_PORT
WS_PORT=$WS_PORT
RPC_BUS_ID=$BUS_ID
DEPLOY_PATH=$DEPLOY_PATH
INSTANCE_DIR=$INSTANCE_DIR
LOG_ROOT=$LOG_ROOT
LOG_DIR=$LOG_DIR
PID_FILE=$PID_FILE
DATA_PATH=$DATA_PATH
EOF

echo "==> 实例 [$NAME] 配置已生成"
echo "    conf    : $CONF_DIR"
echo "    work    : $WORK_PATH"
echo "    logs    : $LOG_DIR"
echo "    data    : $DATA_PATH"
echo "    pid     : $PID_FILE"
echo "    http/ws : $HTTP_PORT / $WS_PORT"
echo "    bus_id  : $BUS_ID"
echo "    process : $PROCESS_NAME"
