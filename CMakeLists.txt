cmake_minimum_required(VERSION 3.14)
project(junqi_recognition VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 桌面编译：使用系统 OpenCV ----
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

add_library(junqi_core STATIC
    src/preprocessor.cpp
    src/detector.cpp
    src/character_extractor.cpp
    src/recognizer.cpp
    src/template_library.cpp
    src/pipeline.cpp
    src/battle_judge.cpp
)
target_include_directories(junqi_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(junqi_core
    PUBLIC
        opencv_core opencv_imgproc opencv_imgcodecs
)

# ---- executables ----

# CLI 工具（ARM + 桌面均编译）
add_executable(junqi_cli src/main.cpp)
target_link_libraries(junqi_cli PRIVATE junqi_core)

# 开发板专项采样工具：按固定顺序定时拍摄并记录识别结果。
add_executable(junqi_capture_test
    tools/board_sequence_capture.cpp
    src/camera_capture.cpp
)
target_link_libraries(junqi_capture_test PRIVATE junqi_core)

# ARM 交叉编译时静态链接 C++ 运行时，避免板子 libstdc++ 版本不兼容
if(CMAKE_CROSSCOMPILING)
    target_link_options(junqi_cli PRIVATE "-static-libstdc++" "-static-libgcc")
    target_link_options(junqi_capture_test PRIVATE
        "-static-libstdc++" "-static-libgcc")
endif()

# 开发工具（仅桌面编译 — 依赖 C++17 <filesystem>，ARM GCC 4.8 不支持）
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(capture_template tools/capture_template.cpp)
    target_compile_features(capture_template PRIVATE cxx_std_17)
    target_link_libraries(capture_template PRIVATE junqi_core)

    add_executable(benchmark tools/benchmark.cpp)
    target_compile_features(benchmark PRIVATE cxx_std_17)
    target_link_libraries(benchmark PRIVATE junqi_core)

    add_executable(debug_pipeline tools/debug_pipeline.cpp)
    target_compile_features(debug_pipeline PRIVATE cxx_std_17)
    target_link_libraries(debug_pipeline PRIVATE junqi_core)

    add_executable(analyze_real_samples tools/analyze_real_samples.cpp)
    target_compile_features(analyze_real_samples PRIVATE cxx_std_17)
    target_link_libraries(analyze_real_samples PRIVATE junqi_core)

    add_executable(score_templates tools/score_templates.cpp)
    target_compile_features(score_templates PRIVATE cxx_std_17)
    target_link_libraries(score_templates PRIVATE opencv_core opencv_imgcodecs)

    add_executable(robustness_test tools/robustness_test.cpp)
    target_compile_features(robustness_test PRIVATE cxx_std_17)
    target_link_libraries(robustness_test PRIVATE junqi_core)

    add_test(NAME synthetic_robustness
             COMMAND robustness_test ${CMAKE_CURRENT_SOURCE_DIR}/templates)
endif()

# ---- Qt GUI application (可选编译) ----
# 桌面编译默认 OFF（需安装 Qt5）；ARM 交叉编译时传 -DBUILD_QT_GUI=ON
option(BUILD_QT_GUI "Build Qt GUI application" OFF)

if(BUILD_QT_GUI)
    # 桌面编译使用系统 Qt5；ARM 交叉编译使用 toolchain 中指定的 Qt5 路径
    if(CMAKE_CROSSCOMPILING)
        find_package(Qt5 COMPONENTS Widgets REQUIRED)
    else()
        find_package(Qt5 COMPONENTS Widgets QUIET)
    endif()

    if(Qt5_FOUND OR Qt5Widgets_FOUND)
        set(CMAKE_AUTOMOC ON)

        add_executable(junqi_gui
            src/qt_main.cpp
            src/main_window.cpp
            include/junqi/main_window.h
            src/processing_worker.cpp
            include/junqi/processing_worker.h
            src/camera_capture.cpp
            include/junqi/camera_capture.h
        )

        target_compile_features(junqi_gui PRIVATE cxx_std_11)
        target_link_libraries(junqi_gui PRIVATE
            junqi_core
            Qt5::Widgets
        )

        # 交叉编译时添加 ARM 库的 rpath-link + 静态链接 C++ 运行时
        if(CMAKE_CROSSCOMPILING)
            if(DEFINED TSLIB_ROOT)
                target_link_directories(junqi_gui PRIVATE "${TSLIB_ROOT}/lib")
                target_link_libraries(junqi_gui PRIVATE ts)
            endif()
            if(DEFINED QT5_ARM_ROOT)
                target_link_options(junqi_gui PRIVATE
                    "-Wl,-rpath-link,${QT5_ARM_ROOT}/lib"
                )
            endif()
            if(DEFINED OPENCV_ARM_PREFIX)
                target_link_options(junqi_gui PRIVATE
                    "-Wl,-rpath-link,${OPENCV_ARM_PREFIX}/lib"
                )
            endif()
            target_link_options(junqi_gui PRIVATE
                "-static-libstdc++"
                "-static-libgcc"
            )
        endif()
    else()
        message(STATUS "Qt5 not found — skipping junqi_gui (desktop build without GUI)")
    endif()
endif()
