#!/bin/bash
# 一键交叉编译 + 打包（仅可执行文件 + 启动脚本，不含依赖库）
# 用法: bash deploy/pack.sh
set -e

# ========== 路径配置（按需修改）==========
BUILD_DIR="/home/cheese/junqi_from_c/junqi/junqi/build_arm"
PKG_ROOT="/tmp/junqi_pkg/root/junqi"
RUNSH_SRC="/home/cheese/junqi_from_c/junqi/junqi/deploy/run.sh"
RUNCLI_SRC="/home/cheese/junqi_from_c/junqi/junqi/deploy/run_cli.sh"
WINDOWS_OUT="/mnt/c/Users/cheese/Desktop/junqi_bin_only.tar.gz"

# ========== 交叉编译 GUI + CLI ==========
cd "${BUILD_DIR}"
make -j$(nproc) junqi_gui junqi_cli 2>&1 | tail -8

# ========== 打包目录 ==========
rm -rf "${PKG_ROOT}"
mkdir -p "${PKG_ROOT}/bin"

# 复制可执行文件
cp junqi_gui junqi_cli "${PKG_ROOT}/bin/"
cp "${RUNSH_SRC}" "${RUNCLI_SRC}" "${PKG_ROOT}/"


# strip 瘦身（交叉编译器配套工具）
arm-none-linux-gnueabi-strip "${PKG_ROOT}/bin/junqi_gui" "${PKG_ROOT}/bin/junqi_cli"

# 最高等级压缩
cd /tmp/junqi_pkg
tar cf - . | gzip -9 > "${WINDOWS_OUT}"

echo "=== Done: $(ls -lh "${WINDOWS_OUT}" | awk '{print $5}') ==="
