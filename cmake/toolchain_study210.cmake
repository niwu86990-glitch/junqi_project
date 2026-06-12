# CMake 交叉编译工具链 — 朱有鹏 study210 (S5PV210, ARM Cortex-A8)
#
# 使用方法:
#   mkdir build_arm && cd build_arm
#   cmake .. \
#     -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_study210.cmake \
#     -DCMAKE_BUILD_TYPE=Release \
#     -DBUILD_QT_GUI=ON
#   make -j$(nproc)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# ---- 交叉编译器 ----
# Sourcery CodeBench Lite 2014.05-29 (GCC 4.8.3)
# 下载: arm-2014.05-29-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2
set(TOOLCHAIN_ROOT "/usr/local/arm/arm-2014.05" CACHE STRING
    "ARM cross-compiler root directory")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_ROOT}/bin/arm-none-linux-gnueabi-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/bin/arm-none-linux-gnueabi-g++")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---- ARM OpenCV 3.4.16 ----
# 来源: opencv3.tar.gz（已解压到 ~/opencv3）
# 包含模块: core, imgproc, imgcodecs（不含 videoio）
set(OpenCV_DIR "/home/cheese/opencv3/share/OpenCV" CACHE STRING
    "ARM OpenCV CMake config directory")

# ---- ARM Qt 5.6.2 ----
# 来源: qt5编译结果/qt5.6.2.zip（已解压到 ~/x210/qt5.6.2）
set(QT5_ARM_ROOT "$ENV{HOME}/x210/qt5.6.2" CACHE STRING
    "ARM Qt5.6.2 install root")

set(CMAKE_PREFIX_PATH "${QT5_ARM_ROOT}")

# ---- tslib 触摸屏 ----
# 来源: qt5编译结果/tslib.zip（已解压到 ~/x210/tslib）
set(TSLIB_ROOT "/home/cheese/x210/tslib" CACHE STRING
    "tslib install root for touchscreen")

# ---- 搜索根路径（确保 CMake 只在 ARM 目录下搜索）----
set(CMAKE_FIND_ROOT_PATH
    "${QT5_ARM_ROOT}"
    "${TSLIB_ROOT}"
    "/home/cheese/opencv3"
)
