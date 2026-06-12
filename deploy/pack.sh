#!/bin/bash
# 一键交叉编译 + 打包（仅可执行文件 + 启动脚本，不含依赖库）
# 用法: bash deploy/pack.sh
set -e

# ========== 项目路径 ==========
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build_arm}"
PKG_ROOT="/tmp/junqi_pkg/root/junqi"
RUNSH_SRC="${SCRIPT_DIR}/run.sh"
RUNCLI_SRC="${SCRIPT_DIR}/run_cli.sh"
WINDOWS_DESKTOP="${WINDOWS_DESKTOP:-}"

if [ -z "${WINDOWS_DESKTOP}" ]; then
    for candidate in \
        "/mnt/c/Users/${USER}/Desktop" \
        "/mnt/c/Users/cheese/Desktop" \
        "/mnt/c/Users/${USER}/桌面"
    do
        if [ -d "${candidate}" ]; then
            WINDOWS_DESKTOP="${candidate}"
            break
        fi
    done
fi

if [ -z "${WINDOWS_DESKTOP}" ] ||
   [ ! -d "${WINDOWS_DESKTOP}" ] ||
   [ ! -w "${WINDOWS_DESKTOP}" ]; then
    echo "错误: 找不到可写的 Windows 桌面目录。"
    echo "可通过 WINDOWS_DESKTOP=/mnt/c/Users/<用户名>/Desktop 指定路径。"
    exit 1
fi
WINDOWS_OUT="${WINDOWS_DESKTOP}/junqi_bin_only.tar.gz"

# ========== 交叉编译 GUI + CLI ==========
cmake --build "${BUILD_DIR}" --target junqi_gui junqi_cli -j"$(nproc)"

# ========== 打包目录 ==========
rm -rf "${PKG_ROOT}"
mkdir -p "${PKG_ROOT}/bin"

# 复制可执行文件
cp "${BUILD_DIR}/junqi_gui" "${BUILD_DIR}/junqi_cli" "${PKG_ROOT}/bin/"
cp "${RUNSH_SRC}" "${RUNCLI_SRC}" "${PKG_ROOT}/"


# strip 瘦身（交叉编译器配套工具）
arm-none-linux-gnueabi-strip "${PKG_ROOT}/bin/junqi_gui" "${PKG_ROOT}/bin/junqi_cli"

# 最高等级压缩
cd /tmp/junqi_pkg
tar cf - . | gzip -9 > "${WINDOWS_OUT}"

echo "=== Done: $(ls -lh "${WINDOWS_OUT}" | awk '{print $5}') ==="
echo "已复制到 Windows 桌面: ${WINDOWS_OUT}"
