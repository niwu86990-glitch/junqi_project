# 军棋棋子图像识别系统 (Junqi Piece Recognition System)

基于 C++ 和 OpenCV 的智能军棋裁判系统，自动识别棋盘照片中的棋子颜色和文字，并判定对战结果。

## 项目状态

| 组件 | 桌面编译 (WSL2/Linux) | ARM 交叉编译 |
|------|:---:|:---:|
| `libjunqi_core.a` (核心算法库) | ✅ 通过 | ✅ 通过 |
| `junqi_cli` (命令行工具) | ✅ 通过 | ✅ 通过 |
| `junqi_gui` (Qt GUI) | ❌ 需 Qt5 | ✅ 通过 |
| `capture_template` (模板采集) | ✅ 通过 | N/A (仅桌面) |
| `benchmark` (准确率评测) | ✅ 通过 | N/A (仅桌面) |
| `debug_pipeline` (调试工具) | ✅ 通过 | N/A (仅桌面) |
| `score_templates` (模板评分) | ✅ 通过 | N/A (仅桌面) |

## 环境要求

### 桌面编译
- Ubuntu 20.04+ / WSL2
- CMake 3.14+
- GCC 9.0+ (支持 C++17)
- OpenCV 4.x (`libopencv-dev`)
- Qt 5.6+ (可选，仅 GUI)

### ARM 交叉编译
- Sourcery CodeBench Lite 2014.05 (arm-none-linux-gnueabi)
- Qt 5.6.2 ARM 预编译包
- OpenCV 3.4 ARM 预编译包 (core/imgproc/imgcodecs)
- tslib 1.4 ARM 预编译包

## 快速开始

### 桌面编译

```bash
# 1. 安装依赖
sudo apt install cmake g++ libopencv-dev

# 2. 编译（核心库 + CLI + 开发工具）
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. 运行 CLI 工具
./junqi_cli <图片路径> [模板目录]

# 示例
./junqi_cli ../test_images/scene_01.jpg ../templates/
```

### ARM 交叉编译

```bash
mkdir build_arm && cd build_arm
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_study210.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_QT_GUI=ON
make -j$(nproc)
```

## 项目结构

```
junqi/
├── CMakeLists.txt              # 构建系统
├── arm-gcc.cmake               # ARM 工具链配置
├── build.sh                    # ARM 交叉编译脚本（手动方式）
├── include/junqi/              # 头文件
│   ├── result.h                #   核心数据结构
│   ├── preprocessor.h          #   图像预处理
│   ├── detector.h              #   棋子检测
│   ├── color_classifier.h      #   颜色分类
│   ├── character_extractor.h   #   字符提取
│   ├── recognizer.h            #   字符识别
│   ├── template_library.h      #   模板库管理
│   ├── pipeline.h              #   识别流水线
│   ├── battle_judge.h          #   对战判定
│   ├── bg_utils.h              #   背景检测工具
│   ├── camera_capture.h        #   V4L2 摄像头直驱
│   ├── main_window.h           #   Qt 主窗口
│   ├── processing_worker.h     #   工作线程
│   └── qt_utils.h              #   QImage ↔ cv::Mat 互转
├── src/                        # 源文件
│   ├── preprocessor.cpp
│   ├── detector.cpp
│   ├── color_classifier.cpp
│   ├── character_extractor.cpp
│   ├── recognizer.cpp
│   ├── template_library.cpp
│   ├── pipeline.cpp
│   ├── battle_judge.cpp
│   ├── camera_capture.cpp
│   ├── main_window.cpp
│   ├── processing_worker.cpp
│   ├── main.cpp                #   CLI 入口
│   └── qt_main.cpp             #   GUI 入口
├── tools/                      # 桌面开发工具
│   ├── benchmark.cpp           #   准确率评测
│   ├── capture_template.cpp    #   模板采集
│   ├── debug_pipeline.cpp      #   流水线调试
│   └── score_templates.cpp     #   模板质量评分
├── templates/                  # 模板库（12 种棋子 × 多张）
├── deploy/                     # ARM 部署脚本
│   ├── run.sh                  #   开发板启动脚本
│   ├── pack.sh                 #   快速编译打包
│   └── package.sh              #   完整部署打包
├── cmake/
│   └── toolchain_study210.cmake # ARM 工具链
├── docs/                       # 项目文档
│   ├── requirements.md         #   需求文档
│   ├── technical_spec.md       #   技术规格
│   ├── design_spec.md          #   设计规范
│   └── execution_guide.md      #   执行指南
└── devlog/                     # 开发日志
    ├── CHANGELOG.md            #   变更日志
    ├── TODO.md                 #   待办事项
    ├── 2026-06-07-arm-cross-compile.md
    └── 2026-06-07-gui-debug.md
```

## 识别流水线

```
输入图像
  │
  ├─ 1. Preprocessor    → 双边滤波去噪 + CLAHE 对比度增强
  ├─ 2. Detector        → 轮廓分析 + 几何约束 → 左右棋子 ROI
  ├─ 3. 对每枚棋子:
  │   ├─ ColorClassifier    → HSV+Lab+YCrCb 三空间投票 → 红/黑
  │   ├─ CharacterExtractor → 笔画提取 + 64×64 归一化
  │   └─ Recognizer         → 模板匹配 (TM_CCOEFF_NORMED) + Hu 矩回退
  └─ 4. BattleJudge     → 根据军棋规则判定胜负
```

## 支持的棋子

| ID | 名称 | ID | 名称 |
|:--:|------|:--:|------|
| 1 | 司令 | 7 | 连长 |
| 2 | 军长 | 8 | 排长 |
| 3 | 师长 | 9 | 工兵 |
| 4 | 旅长 | 10 | 地雷 |
| 5 | 团长 | 11 | 炸弹 |
| 6 | 营长 | 12 | 军旗 |

## B/C 同学改动概要 (2026-06)

| 改动 | 文件 | 说明 |
|------|------|------|
| V4L2 缓冲修复 | `src/camera_capture.cpp` | memcpy 防 mmap 覆写、删 usleep 阻塞、帧丢弃限流 |
| ARM 交叉编译 | `CMakeLists.txt` | 硬编码 ARM 路径、Qt5+tslib 支持 |
| 部署脚本优化 | `deploy/*.sh` | 动态 APP_ROOT、bin/ 子目录、strip 瘦身、OpenCV 库路径补全 |

## 许可

本项目为课程项目，仅供学习和研究使用。
