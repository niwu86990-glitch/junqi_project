#!/bin/bash
# ============================================================
# 打包部署脚本 — 生成 junqi_deploy.tar.gz
# 包含：程序 + Qt5 + OpenCV + tslib + 模板 + 启动脚本（全套运行依赖）
# 在开发机上运行，生成可传输到 ARM 板的部署包
# ============================================================
set -e

# ========== 项目路径 ==========
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
DEPLOY_DIR="${SCRIPT_DIR}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build_arm}"
TEMPLATES_DIR="${PROJECT_ROOT}/templates"
PACKAGE="junqi_deploy"
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

# ARM 第三方库路径
QT5_ROOT="/home/cheese/x210/qt5.6.2"
QT5_LIB="${QT5_ROOT}/lib"
OPENCV_LIB="/home/cheese/opencv3/lib"
TSLIB_ROOT="/home/cheese/x210/tslib"

echo "=== 军棋识别系统部署打包 ==="

if [ ! -x "${BUILD_DIR}/junqi_gui" ] ||
   [ ! -x "${BUILD_DIR}/junqi_cli" ] ||
   [ ! -x "${BUILD_DIR}/junqi_capture_test" ]; then
    echo "错误: 未找到本项目最新的 ARM 可执行文件:"
    echo "  ${BUILD_DIR}/junqi_gui"
    echo "  ${BUILD_DIR}/junqi_cli"
    echo "  ${BUILD_DIR}/junqi_capture_test"
    echo "请先完成 ARM 交叉编译，再重新运行本脚本。"
    exit 1
fi

# 清理上次打包
rm -rf "${DEPLOY_DIR}/${PACKAGE}"
mkdir -p "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/templates"
mkdir -p "${DEPLOY_DIR}/${PACKAGE}/opt/qt5.6.2/lib"
mkdir -p "${DEPLOY_DIR}/${PACKAGE}/opt/opencv3/lib"
mkdir -p "${DEPLOY_DIR}/${PACKAGE}/opt/tslib/lib/ts"
mkdir -p "${DEPLOY_DIR}/${PACKAGE}/opt/tslib/etc"

# ========== 1. 拷贝 ARM 可执行文件 ==========
echo "1. 复制 ARM 可执行文件..."
cp "${BUILD_DIR}/junqi_gui" "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/"
cp "${BUILD_DIR}/junqi_cli" "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/"
cp "${BUILD_DIR}/junqi_capture_test" "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/"
arm-none-linux-gnueabi-strip "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/junqi_gui" 2>/dev/null || true
arm-none-linux-gnueabi-strip "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/junqi_cli" 2>/dev/null || true
arm-none-linux-gnueabi-strip "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/junqi_capture_test" 2>/dev/null || true

# ========== 2. 拷贝模板库 ==========
echo "2. 复制模板库..."
cp -r "${TEMPLATES_DIR}"/* "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/templates/"

# ========== 3. Qt5 运行时库 ==========
echo "3. 复制 Qt5 库..."
# 核心库（GUI 所需全部 .so）
QT5_SO_LIST="
libQt5Core.so.5.6.2
libQt5Gui.so.5.6.2
libQt5Widgets.so.5.6.2
libQt5DBus.so.5.6.2
libQt5Network.so.5.6.2
"
for lib in ${QT5_SO_LIST}; do
    if [ -f "${QT5_LIB}/${lib}" ]; then
        cp -v "${QT5_LIB}/${lib}" "${DEPLOY_DIR}/${PACKAGE}/opt/qt5.6.2/lib/"
    else
        echo "  跳过: ${lib}（不存在）"
    fi
done
# 创建 soname 符号链接
echo "  创建 Qt5 符号链接..."
cd "${DEPLOY_DIR}/${PACKAGE}/opt/qt5.6.2/lib"
for lib_full in *.so.*.*; do
    [ -e "$lib_full" ] || continue
    base=$(echo "$lib_full" | sed 's/\.so\..*/.so/')
    major=$(echo "$lib_full" | sed 's/\(\.so\.[0-9]*\)\..*/\1/')
    # 例: libQt5Core.so.5.6.2 → libQt5Core.so.5 → libQt5Core.so
    [ ! -e "$major" ] && ln -sf "$lib_full" "$major"
    [ ! -e "$base" ] && ln -sf "$lib_full" "$base"
done
cd -

# ========== 4. OpenCV 库 ==========
echo "4. 复制 OpenCV 库..."
for lib in libopencv_core.so.3.4.16 libopencv_imgproc.so.3.4.16 libopencv_imgcodecs.so.3.4.16; do
    cp -v "${OPENCV_LIB}/${lib}" "${DEPLOY_DIR}/${PACKAGE}/opt/opencv3/lib/"
done
# 创建 soname 符号链接
cd "${DEPLOY_DIR}/${PACKAGE}/opt/opencv3/lib"
ln -sf libopencv_core.so.3.4.16 libopencv_core.so.3.4
ln -sf libopencv_core.so.3.4 libopencv_core.so
ln -sf libopencv_imgproc.so.3.4.16 libopencv_imgproc.so.3.4
ln -sf libopencv_imgproc.so.3.4 libopencv_imgproc.so
ln -sf libopencv_imgcodecs.so.3.4.16 libopencv_imgcodecs.so.3.4
ln -sf libopencv_imgcodecs.so.3.4 libopencv_imgcodecs.so
cd -

# ========== 5. tslib 触摸库 ==========
echo "5. 复制 tslib..."
cp -v "${TSLIB_ROOT}/lib/libts-1.4.so.0.2.4" "${DEPLOY_DIR}/${PACKAGE}/opt/tslib/lib/"
cd "${DEPLOY_DIR}/${PACKAGE}/opt/tslib/lib"
ln -sf libts-1.4.so.0.2.4 libts-1.4.so.0
ln -sf libts-1.4.so.0 libts.so
cd -
cp -rv "${TSLIB_ROOT}/lib/ts"/*.so "${DEPLOY_DIR}/${PACKAGE}/opt/tslib/lib/ts/"
cp -v "${TSLIB_ROOT}/etc/ts.conf" "${DEPLOY_DIR}/${PACKAGE}/opt/tslib/etc/" 2>/dev/null || true

# ========== 6. 字体文件 ==========
echo "6. 复制中文字体..."
mkdir -p "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/fonts"
cp -v "${DEPLOY_DIR}/fonts.ttf" "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/fonts/AlibabaPuHuiTi-3-55-Regular.ttf"

# ========== 7. 启动脚本 ==========
echo "7. 复制启动脚本..."
cp "${DEPLOY_DIR}/run.sh" \
   "${DEPLOY_DIR}/run_cli.sh" \
   "${DEPLOY_DIR}/run_capture_test.sh" \
   "${DEPLOY_DIR}/${PACKAGE}/opt/junqi/"

# ========== 8. Qt linuxfb 平台插件 ==========
echo "8. 复制 Qt 平台插件 (linuxfb)..."
mkdir -p "${DEPLOY_DIR}/${PACKAGE}/opt/qt5.6.2/plugins/platforms"
cp -v "${QT5_ROOT}/plugins/platforms/libqlinuxfb.so" \
    "${DEPLOY_DIR}/${PACKAGE}/opt/qt5.6.2/plugins/platforms/"

# ========== 9. 打包 ==========
echo "9. 打包..."
cd "${DEPLOY_DIR}/${PACKAGE}"
tar czf "../${PACKAGE}.tar.gz" .
cd -

echo ""
echo "===== 打包完成 ====="
ls -lh "${DEPLOY_DIR}/${PACKAGE}.tar.gz"

# ========== 10. 复制到 Windows 桌面 ==========
if [ -z "${WINDOWS_DESKTOP}" ] ||
   [ ! -d "${WINDOWS_DESKTOP}" ] ||
   [ ! -w "${WINDOWS_DESKTOP}" ]; then
    echo "错误: 找不到可写的 Windows 桌面目录。"
    echo "可通过 WINDOWS_DESKTOP=/mnt/c/Users/<用户名>/Desktop 指定路径。"
    exit 1
fi
cp -f "${DEPLOY_DIR}/${PACKAGE}.tar.gz" "${WINDOWS_DESKTOP}/"
echo ""
echo "已复制到 Windows 桌面: ${WINDOWS_DESKTOP}/${PACKAGE}.tar.gz"

echo ""
echo "部署步骤:"
echo "  1. 将 ${PACKAGE}.tar.gz 放入 192.168.1.30 的 TFTP 根目录"
echo "  2. 开发板执行: tftp -g -r ${PACKAGE}.tar.gz 192.168.1.30"
echo "  3. 开发板执行: gunzip ${PACKAGE}.tar.gz"
echo "  4. 开发板执行: cd / && tar xf /root/${PACKAGE}.tar -C /"
echo "  5. 开发板执行: sh /opt/junqi/run.sh"
