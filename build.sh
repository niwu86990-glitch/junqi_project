#!/bin/bash
# ARM 交叉编译脚本（手动方式，备选）
# 主要用于调试或当 CMake toolchain 不可用时
# 推荐使用 CMake + cmake/toolchain_study210.cmake 方式

CXX=/usr/local/arm/arm-2014.05/bin/arm-none-linux-gnueabi-g++
QT_DIR=/home/cheese/x210/qt5.6.2
OPENCV_DIR=/home/cheese/opencv3
TSLIB_DIR=/home/cheese/x210/tslib

mkdir -p build_arm

$CXX -fPIC -g -O2 -std=c++11 \
    -I./include \
    -I${QT_DIR}/include \
    -I${OPENCV_DIR}/include \
    -I${TSLIB_DIR}/include \
    -L${QT_DIR}/lib \
    -L${OPENCV_DIR}/lib \
    -L${TSLIB_DIR}/lib \
    src/battle_judge.cpp \
    src/camera_capture.cpp \
    src/character_extractor.cpp \
    src/detector.cpp \
    src/main_window.cpp \
    src/pipeline.cpp \
    src/preprocessor.cpp \
    src/processing_worker.cpp \
    src/qt_main.cpp \
    src/recognizer.cpp \
    src/template_library.cpp \
    -o build_arm/junqi_gui \
    -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
    -lQt5Core -lQt5Gui -lQt5Widgets \
    -lts -lpthread -ldl

if [ $? -eq 0 ]; then
    echo "编译成功: build_arm/junqi_gui"
else
    echo "编译失败，请检查错误信息"
fi
