#!/usr/bin/env bash
#
# package-cluster.sh — 一次打包 N 个实例（每个实例的配置已渲染好）
#
# 用法:
#   ./scripts/cluster/package-cluster.sh [ENV] [选项]
#
#   ENV                      使用 bin/conf/<ENV>.env 渲染，默认 prod
#
# 选项:
#   --instances N            实例数量（默认 1），自动命名 server_1、server_2…
#   --names a,b,c            自定义实例名（优先于 --instances）
#   --deploy-path PATH       部署根目录（程序/配置，默认 /opt/techhaven）
#   --log-root PATH          日志根目录（默认 /data/apps，仅日志单独存放）
#   --data-path PATH         备份/导出共享目录（默认 <deploy-path>/share/data）
#   --http-base PORT         起始 HTTP 端口（默认 8090，每实例 +100）
#   --ws-base PORT           起始 WS 端口（默认 8091，每实例 +100）
#   --bus-prefix PREFIX      rpc.bus_id 前三段（默认 1.1.2，末段=实例编号）
#   --env-file PATH          指定 env 文件（默认 bin/conf/<ENV>.env）
#
# 示例:
#   ./scripts/cluster/package-cluster.sh prod --instances 3
#   产物内含 server_1、server_2、server_3（实例目录直接位于部署根目录）；日志在 /data/apps/<name>/logs
#
# 前置:
#   - 已执行 make build（bin/server、bin/module/*.so）
#   - chen-sdk-*/lib/libchen.so、errors.json、locale/*.mo 存在
#   - bin/conf/<ENV>.env 存在（渲染需要真实连接信息）
#
# 产物:
#   dist-cluster/                                        # 可检查的部署目录
#   techhaven-cluster-<version>-<arch>-<ts>.tar.gz
#   techhaven-cluster-<version>-<arch>-<ts>.tar.gz.sha256
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

ENV_NAME="prod"
INSTANCES_ARG=""
NAMES_ARG=""
DEPLOY_PATH="/opt/techhaven"
ARG_LOG_ROOT=""
ARG_DATA_PATH=""
HTTP_BASE=8090
WS_BASE=8091
BUS_PREFIX="1.1.2"
ARG_ENV_FILE=""

usage() { sed -n '2,37p' "$0"; }
log() { echo "==> $*"; }
die() { echo "[package][ERROR] $*" >&2; exit 1; }

# 确保 gettext 目录已编译（缺失时自动尝试 ninja i18n / i18n.py build）
ensure_mo_files() {
    local mo_dir="$ROOT_DIR/locale/C/LC_MESSAGES"
    if [ -f "$mo_dir/techhaven_en.mo" ] && [ -f "$mo_dir/techhaven_zh_CN.mo" ]; then
        return 0
    fi
    log "locale/*.mo 缺失，尝试编译（ninja i18n / scripts/i18n.py build）"
    if [ -d "$ROOT_DIR/build" ] && command -v ninja >/dev/null 2>&1; then
        ninja -C "$ROOT_DIR/build" i18n >/dev/null 2>&1 || true
    fi
    if [ ! -f "$mo_dir/techhaven_en.mo" ] && command -v python3 >/dev/null 2>&1; then
        python3 "$ROOT_DIR/scripts/i18n.py" --project-dir "$ROOT_DIR" build >/dev/null 2>&1 || true
    fi
    [ -f "$mo_dir/techhaven_en.mo" ] && [ -f "$mo_dir/techhaven_zh_CN.mo" ] && return 0
    die "缺少 locale/C/LC_MESSAGES/*.mo：请在构建机安装 gettext 后执行 make i18n（或 ninja -C build i18n）再打包"
}

# 第一个非选项参数作为 ENV
if [ $# -gt 0 ] && [ "${1#-}" = "$1" ]; then
    ENV_NAME="$1"
    shift
fi

while [ $# -gt 0 ]; do
    case "$1" in
        --instances)      INSTANCES_ARG="${2:-}"; shift 2 ;;
        --instances=*)    INSTANCES_ARG="${1#*=}"; shift ;;
        --names)          NAMES_ARG="${2:-}"; shift 2 ;;
        --names=*)        NAMES_ARG="${1#*=}"; shift ;;
        --deploy-path)    DEPLOY_PATH="${2:-}"; shift 2 ;;
        --deploy-path=*)  DEPLOY_PATH="${1#*=}"; shift ;;
        --log-root)       ARG_LOG_ROOT="${2:-}"; shift 2 ;;
        --log-root=*)     ARG_LOG_ROOT="${1#*=}"; shift ;;
        --data-path)      ARG_DATA_PATH="${2:-}"; shift 2 ;;
        --data-path=*)    ARG_DATA_PATH="${1#*=}"; shift ;;
        --http-base)      HTTP_BASE="${2:-}"; shift 2 ;;
        --http-base=*)    HTTP_BASE="${1#*=}"; shift ;;
        --ws-base)        WS_BASE="${2:-}"; shift 2 ;;
        --ws-base=*)      WS_BASE="${1#*=}"; shift ;;
        --bus-prefix)     BUS_PREFIX="${2:-}"; shift 2 ;;
        --bus-prefix=*)   BUS_PREFIX="${1#*=}"; shift ;;
        --env-file)       ARG_ENV_FILE="${2:-}"; shift 2 ;;
        --env-file=*)     ARG_ENV_FILE="${1#*=}"; shift ;;
        -h|--help)        usage; exit 0 ;;
        *) die "未知参数: $1（--help 查看用法）" ;;
    esac
done

# ---- 实例名 / 端口 ----
instance_names=()
if [ -n "$NAMES_ARG" ]; then
    IFS=',' read -r -a instance_names <<< "$NAMES_ARG"
else
    count="${INSTANCES_ARG:-1}"
    case "$count" in
        ''|*[!0-9]*) die "--instances 必须是正整数" ;;
    esac
    [ "$count" -ge 1 ] || die "--instances 至少为 1"
    for i in $(seq 1 "$count"); do
        instance_names+=("server_$i")
    done
fi

for n in "${instance_names[@]}"; do
    case "$n" in
        ''|*[!A-Za-z0-9_-]*) die "非法实例名: '$n'（只允许字母/数字/_/-）" ;;
        conf|module|lib|scripts|nginx|docs|locale|share|server|errors.json|VERSION)
            die "实例名 '$n' 与部署根目录的既有文件/目录冲突" ;;
    esac
done

case "$DEPLOY_PATH" in
    /*) ;;
    *) die "--deploy-path 必须是绝对路径（例如 /opt/techhaven）" ;;
esac

LOG_ROOT="${ARG_LOG_ROOT:-/data/apps}"
DATA_PATH="${ARG_DATA_PATH:-$DEPLOY_PATH/share/data}"
ENV_FILE="${ARG_ENV_FILE:-$ROOT_DIR/bin/conf/$ENV_NAME.env}"

# ---- 前置检查 ----
[ -x "$ROOT_DIR/bin/server" ] || die "缺少 bin/server，请先执行 make build"
[ -f "$ROOT_DIR/errors.json" ] || die "缺少 errors.json"
ensure_mo_files
if [ ! -f "$ENV_FILE" ]; then
    echo "[package][ERROR] 缺少 env 文件: $ENV_FILE" >&2
    echo "  实例配置需要在打包时写入真实连接信息，请先准备 env：" >&2
    echo "    cp bin/conf/prod.env.example bin/conf/prod.env && vi bin/conf/prod.env" >&2
    echo "  或指定其他 env 文件：" >&2
    echo "    $0 $ENV_NAME --env-file /path/to/prod.env" >&2
    echo "  仅本机联调也可以直接用 dev： $0 dev --instances ${INSTANCES_ARG:-1}" >&2
    exit 1
fi

CHEN_SDK_LIB=""
for lib in "$ROOT_DIR"/chen-sdk-*/lib/libchen.so; do
    [ -f "$lib" ] && { CHEN_SDK_LIB="$lib"; break; }
done
[ -n "$CHEN_SDK_LIB" ] || die "找不到 chen-sdk-*/lib/libchen.so"

shopt -s nullglob
module_sos=("$ROOT_DIR"/bin/module/*.so)
shopt -u nullglob
[ ${#module_sos[@]} -gt 0 ] || die "bin/module/ 下没有 .so 模块"

ARCH="$(uname -m)"
VERSION="$(git -C "$ROOT_DIR" describe --tags --always --dirty 2>/dev/null || echo unknown)"
TS="$(date +%Y%m%d%H%M%S)"
PKG_BASENAME="techhaven-cluster-${VERSION}-${ARCH}-${TS}"
PKG_PATH="$ROOT_DIR/${PKG_BASENAME}.tar.gz"
STAGE_DIR="$ROOT_DIR/dist-cluster"

log "实例: ${instance_names[*]}  程序目录: $DEPLOY_PATH  日志目录: $LOG_ROOT/<name>/logs  数据目录: $DATA_PATH"
log "打包版本: $VERSION ($ARCH, env=$ENV_NAME)"

# ---- 组装目录 ----
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"/{conf,module,lib,scripts,nginx,docs}

cp -p "$ROOT_DIR/bin/server" "$STAGE_DIR/server"
chmod +x "$STAGE_DIR/server"
for so in "${module_sos[@]}"; do
    cp -p "$so" "$STAGE_DIR/module/"
done
cp -p "$CHEN_SDK_LIB" "$STAGE_DIR/lib/libchen.so"

cp -p "$ROOT_DIR/errors.json" "$STAGE_DIR/errors.json"
mkdir -p "$STAGE_DIR/locale/C/LC_MESSAGES"
cp -p "$ROOT_DIR/locale/C/LC_MESSAGES/"*.mo "$STAGE_DIR/locale/C/LC_MESSAGES/"

# 模板 + env 示例（不打包真实 env，避免泄漏密钥）
for tpl in "$ROOT_DIR"/bin/conf/*.yml.tpl; do
    [ -f "$tpl" ] || continue
    cp -p "$tpl" "$STAGE_DIR/conf/"
done
for envf in "$ROOT_DIR"/bin/conf/*.env.example; do
    [ -f "$envf" ] || continue
    cp -p "$envf" "$STAGE_DIR/conf/"
done

# 控制脚本
cp -p "$SCRIPT_DIR/render-instance.sh" "$STAGE_DIR/scripts/render-instance.sh"
cp -p "$SCRIPT_DIR/clusterctl.sh" "$STAGE_DIR/scripts/clusterctl.sh"
chmod +x "$STAGE_DIR/scripts/render-instance.sh" "$STAGE_DIR/scripts/clusterctl.sh"
[ -f "$SCRIPT_DIR/nginx.example.conf" ] && cp -p "$SCRIPT_DIR/nginx.example.conf" "$STAGE_DIR/nginx/techhaven.conf.example"
[ -f "$SCRIPT_DIR/README-cluster.md" ] && cp -p "$SCRIPT_DIR/README-cluster.md" "$STAGE_DIR/README-cluster.md"
[ -f "$ROOT_DIR/docs/multi-instance.md" ] && cp -p "$ROOT_DIR/docs/multi-instance.md" "$STAGE_DIR/docs/multi-instance.md"

# ---- 逐实例渲染配置 ----
idx=0
for name in "${instance_names[@]}"; do
    idx=$((idx + 1))
    http_port=$((HTTP_BASE + (idx - 1) * 100))
    ws_port=$((WS_BASE + (idx - 1) * 100))
    render_args=(
        --root "$STAGE_DIR"
        --deploy-path "$DEPLOY_PATH"
        --log-root "$LOG_ROOT"
        --data-path "$DATA_PATH"
        --env-file "$ENV_FILE"
        --name "$name"
        --instance-id "$idx"
        --http-port "$http_port"
        --ws-port "$ws_port"
        --bus-id "$BUS_PREFIX.$idx"
    )
    "$STAGE_DIR/scripts/render-instance.sh" "${render_args[@]}"
done

# ---- 版本信息 ----
cat > "$STAGE_DIR/VERSION" <<EOF
version=$VERSION
arch=$ARCH
env=$ENV_NAME
deploy_path=$DEPLOY_PATH
log_root=$LOG_ROOT
data_path=$DATA_PATH
instances=${instance_names[*]}
built_at=$(date '+%Y-%m-%d %H:%M:%S %z')
git_commit=$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)
EOF

# ---- 打 tar + 校验和 ----
rm -f "$PKG_PATH" "$PKG_PATH.sha256"
tar czf "$PKG_PATH" -C "$STAGE_DIR" .
if command -v sha256sum >/dev/null 2>&1; then
    ( cd "$ROOT_DIR" && sha256sum "$(basename "$PKG_PATH")" > "$(basename "$PKG_PATH").sha256" )
else
    ( cd "$ROOT_DIR" && shasum -a 256 "$(basename "$PKG_PATH")" > "$(basename "$PKG_PATH").sha256" )
fi

log "打包完成"
echo "    实例    : ${instance_names[*]}（配置已生成到 <name>/conf）"
echo "    产物目录: $STAGE_DIR"
echo "    压缩包  : $PKG_PATH"
echo "    校验和  : $PKG_PATH.sha256"
echo
echo "部署端（Linux）:"
echo "    mkdir -p $DEPLOY_PATH && tar xzf $(basename "$PKG_PATH") -C $DEPLOY_PATH"
echo "    cd $DEPLOY_PATH && ./scripts/clusterctl.sh start all"
echo "    ./scripts/clusterctl.sh health all"
