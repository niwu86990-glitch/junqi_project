# 2026-06-07 — ARM 交叉编译 & Qt GUI 嵌入式部署

## 概述

将 C++ + OpenCV 军棋识别系统移植到朱有鹏 study210 ARM 开发板（S5PV210 Cortex-A8）。
目标：触摸屏 GUI 应用，点击按钮触发摄像头拍照，自动识别左右棋子并判定胜负。

---

## 一、环境信息

| 项 | 值 |
|----|-----|
| 目标板 | 朱有鹏 study210，Samsung S5PV210 Cortex-A8 |
| 屏幕 | 1024×600 LCD |
| 摄像头 | USB 摄像头，800×600 |
| 交叉编译器 | Sourcery CodeBench Lite 2014.05-29（GCC 4.8.3） |
| 编译器路径 | `/usr/local/arm/arm-2014.05/bin/arm-none-linux-gnueabi-g++` |
| ARM Qt5 | `/home/cheese/x210/qt5.6.2/`（Qt 5.6.2，预编译 ARM 版） |
| ARM OpenCV | `/home/cheese/opencv3/`（OpenCV 3.4.16，仅 core/imgproc/imgcodecs） |
| tslib | `/home/cheese/x210/tslib/`（触摸屏驱动库 1.4） |
| 开发板 IP | 192.168.1.20 |
| 开发机 IP | 192.168.1.30 |

---

## 二、新增模块

### 2.1 BattleJudge — 对战判定

**文件**：[`include/junqi/battle_judge.h`](include/junqi/battle_judge.h)、[`src/battle_judge.cpp`](src/battle_judge.cpp)

纯逻辑模块，无外部依赖。根据军棋规则判定双方棋子对战结果：

| 条件 | 结果 |
|------|------|
| 任一识别失败 | `INVALID` |
| 炸弹(11) vs 任意 | `DRAW`（同归于尽） |
| 相同棋子 | `DRAW` |
| 地雷(10) vs 工兵(9) | 工兵胜（工兵挖雷） |
| 地雷(10) vs 其他 | 地雷胜 |
| 军旗(12) vs 任意 | 对方胜（军旗最弱） |
| 普通比较 | ID 小的胜（司令=1 最大） |

### 2.2 CameraCapture — V4L2 摄像头直驱

**文件**：[`include/junqi/camera_capture.h`](include/junqi/camera_capture.h)、[`src/camera_capture.cpp`](src/camera_capture.cpp)

**关键决策**：不使用 OpenCV 的 `cv::VideoCapture`（需要 `libopencv_videoio.so`，ARM 预编译包不含此模块），改用 Linux V4L2 内核 API 直接操作摄像头。

V4L2 采集流程：
```
open("/dev/video0") → VIDIOC_QUERYCAP → VIDIOC_S_FMT(YUYV 640×480)
→ VIDIOC_REQBUFS(4个) → mmap (内存映射) → VIDIOC_QBUF
→ VIDIOC_STREAMON
→ 丢前4帧（曝光稳定） → 返回第5帧
→ YUYV→BGR (cv::cvtColor, COLOR_YUV2BGR_YUYV)
→ 关闭时: STREAMOFF → munmap → close
```

关键实现细节：
- 使用 `ioctl(fd, VIDIOC_DQBUF, &buf)` 阻塞等待帧
- 所有缓冲区通过 `mmap` 映射到用户空间，零拷贝
- YUYV 是 YUV 4:2:2 交错格式，2 字节/像素，直接用 `cv::Mat(height, width, CV_8UC2)` 包裹后转换

### 2.3 ProcessingWorker — 工作线程

**文件**：[`include/junqi/processing_worker.h`](include/junqi/processing_worker.h)、[`src/processing_worker.cpp`](src/processing_worker.cpp)

基于 Qt `QObject::moveToThread` 模式的工作线程：

```
MainWindow (主线程)              ProcessingWorker (工作线程)
     │                                    │
     │── button.clicked ──────────────────→ process()
     │                                    │  1. CameraCapture::capture()
     │                                    │  2. Pipeline::process(frame)
     │                                    │  3. BattleJudge::judge(left, right)
     │   onResultReady() ←─── emit ───────│  4. emit resultReady(...)
     │   更新 UI                            │
```

关键设计：
- 信号槽使用 `Qt::QueuedConnection`（跨线程自动排队）
- 自定义类型 `RecognitionOutput`、`BattleResult` 通过 `Q_DECLARE_METATYPE` + `qRegisterMetaType` 注册
- 按钮点击后立即 `setEnabled(false)`，结果返回后恢复（防抖）

### 2.4 MainWindow — Qt GUI 主窗口

**文件**：[`include/junqi/main_window.h`](include/junqi/main_window.h)、[`src/main_window.cpp`](src/main_window.cpp)、[`src/qt_main.cpp`](src/qt_main.cpp)

竖屏布局（适配 1024×600 触摸屏）：
```
┌──────────────────────────┐
│    军棋对战识别系统        │
├──────────────────────────┤
│                          │
│   [   📷 截图判定   ]     │  ← 大按钮，手指点击友好
│                          │
├──────────────────────────┤
│  红方: 司令  (0.92)      │
│  黑方: 军长  (0.88)      │
│  ────────────────────── │
│  判定: ★ 红方获胜 ★      │
├──────────────────────────┤
│  状态: 就绪 / 处理中...   │
└──────────────────────────┘
```

CLI 参数：
- `--template-dir <path>`：模板库目录
- `--camera <device>`：摄像头设备路径（默认 `/dev/video0`）

---

## 三、C++ 标准降级：C++17 → C++11

ARM GCC 4.8.3 最高支持 C++11，因此做了以下修改：

### 3.1 std::filesystem → POSIX

| 原 C++17 写法 | 替换为 (C++11 + POSIX) |
|---------------|------------------------|
| `fs::exists(path)` | `stat(path.c_str(), &st) == 0` |
| `fs::is_directory(path)` | `S_ISDIR(st.st_mode)` |
| `fs::directory_iterator(dir)` | `opendir()` / `readdir()` / `closedir()` |
| `entry.is_directory()` | `entry->d_type == DT_DIR`（`DT_UNKNOWN` 时 fallback `stat`） |
| `entry.path().filename().string()` | `entry->d_name` |
| `entry.path().extension()` | `path.rfind('.')` + `path.substr(dot)` |
| `fs::remove(path)` | `remove(path.c_str())` |

受影响文件：
- [`src/template_library.cpp`](src/template_library.cpp) — 模板库加载，遍历目录树
- [`tools/score_templates.cpp`](tools/score_templates.cpp) — 模板评分工具

### 3.2 std::make_unique → new

```cpp
// C++14
auto p = std::make_unique<Pipeline>(config);

// C++11
std::unique_ptr<Pipeline> p(new Pipeline(config));
// 或
pipeline_.reset(new Pipeline(config));
```

受影响文件：[`src/processing_worker.cpp`](src/processing_worker.cpp)

### 3.3 添加 `<memory>` 头文件

GCC 4.8.3 的 `<memory>` 不会被其他头文件间接包含，`processing_worker.h` 中使用了 `std::unique_ptr` 必须显式 `#include <memory>`。

---

## 四、CMake 交叉编译配置

### 4.1 工具链文件

**文件**：[`cmake/toolchain_study210.cmake`](cmake/toolchain_study210.cmake)

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 编译器
set(TOOLCHAIN_ROOT "/usr/local/arm/arm-2014.05")
set(CMAKE_C_COMPILER   "${TOOLCHAIN_ROOT}/bin/arm-none-linux-gnueabi-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/bin/arm-none-linux-gnueabi-g++")

# 搜索策略：只在 ARM 目录下搜索
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 第三方库路径
set(OpenCV_DIR "/home/cheese/opencv3/share/OpenCV")
set(QT5_ARM_ROOT "$ENV{HOME}/x210/qt5.6.2")
set(TSLIB_ROOT "/home/cheese/x210/tslib")

# 搜索根路径
set(CMAKE_FIND_ROOT_PATH "${QT5_ARM_ROOT}" "${TSLIB_ROOT}" "/home/cheese/opencv3")
```

### 4.2 CMakeLists.txt 主要改动

**文件**：[`CMakeLists.txt`](CMakeLists.txt)

1. C++ 标准：`CMAKE_CXX_STANDARD 17` → `11`
2. OpenCV 组件：移除 `videoio`，只保留 `core imgproc imgcodecs`
3. `junqi_gui` 目标：
   - 头文件显式列入 `add_executable()`（CMake AUTOMOC 必须扫描到 Q_OBJECT 头文件）
   - 交叉编译时添加 `rpath-link`：
     ```cmake
     target_link_options(junqi_gui PRIVATE
         "-Wl,-rpath-link,${QT5_ARM_ROOT}/lib"
         "-Wl,-rpath-link,${TSLIB_ROOT}/lib"
         "-Wl,-rpath-link,/home/cheese/opencv3/lib"
     )
     ```
4. 桌面工具（`capture_template`、`benchmark`、`debug_pipeline`、`score_templates`）包在 `if(NOT CMAKE_CROSSCOMPILING)` 中，ARM 编译时跳过

### 4.3 编译命令

```bash
mkdir -p build_arm && cd build_arm
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_study210.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_QT_GUI=ON
make -j$(nproc)
```

---

## 五、构建产物

| 产物 | 路径 | 架构 | 大小 |
|------|------|------|------|
| `junqi_gui` | `build_arm/junqi_gui` | ARM 32-bit EABI5 | 206 KB |
| `junqi_cli` | `build_arm/junqi_cli` | ARM 32-bit EABI5 | 143 KB |
| `libjunqi_core.a` | `build_arm/libjunqi_core.a` | ARM 32-bit | 200 KB |

运行时依赖（`readelf -d` 确认）：
```
libQt5Widgets.so.5  → libQt5Gui.so.5  → libQt5Core.so.5
                                       → libts-1.4.so.0
libopencv_imgcodecs.so.3.4
libopencv_imgproc.so.3.4
libopencv_core.so.3.4
libstdc++.so.6, libm.so.6, libgcc_s.so.1, libc.so.6
```

Qt 平台插件：`libqlinuxfb.so`（直接写 `/dev/fb0`，无需 X11/Wayland）

---

## 六、遇到的问题及解决

### 问题 1：GCC 4.8.3 不支持 C++17

**症状**：`CMAKE_CXX_STANDARD 17` → CMake Error: "The compiler does not support C++17"

**解决**：降级到 C++11，替换所有 C++14/17 特性（见第三章）

### 问题 2：`std::filesystem` 头文件不存在

**症状**：`fatal error: filesystem: No such file or directory`

**原因**：`<filesystem>` 是 C++17 标准库，GCC 4.8.3 的 libstdc++ 不含此头文件

**解决**：用 POSIX `<dirent.h>` + `<sys/stat.h>` 重写所有目录遍历和文件操作

### 问题 3：MOC 生成空的 mocs_compilation.cpp

**症状**：链接时报 `undefined reference to vtable for MainWindow`、`vtable for ProcessingWorker`

**原因**：CMake AUTOMOC 扫描 `add_executable()` 中的源文件列表，只找到 `.cpp` 文件，没有 `.h` 文件，所以认为 "No files found that require moc"，生成了空的 moc 文件

**解决**：把包含 `Q_OBJECT` 宏的头文件显式加入 `add_executable()` 的源文件列表：
```cmake
add_executable(junqi_gui
    src/qt_main.cpp
    src/main_window.cpp
    include/junqi/main_window.h        # ← 必须显式列出
    src/processing_worker.cpp
    include/junqi/processing_worker.h  # ← 必须显式列出
    src/camera_capture.cpp
    include/junqi/camera_capture.h
)
```

### 问题 4：tslib 链接警告

**症状**：`ld: warning: libts-1.4.so.0, needed by libQt5Widgets.so.5.6.2, not found`

**原因**：工具链文件中 `TSLIB_ROOT` 指向 `/home/cheese/tslib`（有 Zone.Identifier 残留的重复副本），但 `rpath-link` 路径和其他地方不一致；实际上是 `rpath-link` 路径配置问题

**解决**：删除 `/home/cheese/tslib`，统一 tslib 路径为 `/home/cheese/x210/tslib`，警告消失

### 问题 5：opencv_videoio 不存在

**症状**：`find_package(OpenCV)` 找不到 `opencv_videoio` 组件

**原因**：ARM 预编译的 OpenCV 只包含 core、imgproc、imgcodecs 三个模块

**解决**：用 V4L2 直驱摄像头，完全移除对 `cv::VideoCapture` 和 `opencv_videoio` 的依赖

---

## 七、部署方案

### 7.1 部署包结构

**生成脚本**：[`deploy/package.sh`](deploy/package.sh)
**产物**：[`deploy/junqi_deploy.tar.gz`](deploy/junqi_deploy.tar.gz)（24 MB，107 个文件）

```
opt/
├── junqi/
│   ├── junqi_gui              ← ARM 可执行文件 (strip 后约 150KB)
│   ├── run.sh                 ← 启动脚本（设置 LD_LIBRARY_PATH + tslib + Qt 环境）
│   └── templates/             ← 12 个棋子目录，每个 5-8 张 64×64 PNG
├── qt5.6.2/
│   ├── lib/
│   │   ├── libQt5Core.so.5 -> libQt5Core.so.5.6.2
│   │   ├── libQt5Gui.so.5 -> libQt5Gui.so.5.6.2
│   │   └── libQt5Widgets.so.5 -> libQt5Widgets.so.5.6.2
│   └── plugins/platforms/
│       └── libqlinuxfb.so     ← Linux framebuffer 显示插件
├── opencv3/lib/
│   ├── libopencv_core.so.3.4 -> libopencv_core.so.3.4.16
│   ├── libopencv_imgproc.so.3.4 -> libopencv_imgproc.so.3.4.16
│   └── libopencv_imgcodecs.so.3.4 -> libopencv_imgcodecs.so.3.4.16
└── tslib/
    ├── lib/
    │   ├── libts.so -> libts-1.4.so.0 -> libts-1.4.so.0.2.4
    │   └── ts/                ← 20 个滤波插件（input/linear/dejitter/pthres...）
    └── etc/ts.conf            ← tslib 配置文件
```

### 7.2 启动脚本环境变量说明

```bash
# Qt 显示
QT_QPA_PLATFORM=linuxfb           # 直接写 /dev/fb0，嵌入式 framebuffer 显示
QT_QPA_PLATFORM_PLUGIN_PATH=...   # 找 libqlinuxfb.so 的路径
QT_QPA_FB_TSLIB=1                 # 启用 tslib 触摸屏支持
QT_QPA_FB_HIDECURSOR=1            # 隐藏鼠标光标

# tslib 触摸屏
TSLIB_TSDEVICE=/dev/input/event0  # 触摸屏输入设备
TSLIB_CALIBFILE=/etc/pointercal   # 校准数据文件
TSLIB_CONFFILE=/opt/tslib/etc/ts.conf
TSLIB_PLUGINDIR=/opt/tslib/lib/ts # 滤波插件目录
```

---

## 八、完整文件清单（本次会话涉及）

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/junqi/battle_judge.h` | 新增 | 判定算法头文件 |
| `src/battle_judge.cpp` | 新增 | 判定算法实现 |
| `include/junqi/camera_capture.h` | 新增 | V4L2 摄像头头文件 |
| `src/camera_capture.cpp` | 新增 | V4L2 摄像头实现 |
| `include/junqi/processing_worker.h` | 新增 | 工作线程头文件 |
| `src/processing_worker.cpp` | 新增 | 工作线程实现 |
| `include/junqi/main_window.h` | 新增 | Qt 主窗口头文件 |
| `src/main_window.cpp` | 新增 | Qt 主窗口实现 |
| `src/qt_main.cpp` | 新增 | Qt 应用入口 |
| `cmake/toolchain_study210.cmake` | 新增 | ARM 交叉编译工具链 |
| `src/template_library.cpp` | 重写 | C++17→C++11 (POSIX) |
| `tools/score_templates.cpp` | 重写 | C++17→C++11 (POSIX) |
| `CMakeLists.txt` | 修改 | C++11 + Qt GUI 目标 + 交叉编译配置 |
| `deploy/run.sh` | 新增 | 开发板启动脚本 |
| `deploy/package.sh` | 新增 | 部署打包脚本 |
| `devlog/TODO.md` | 新增 | 用户待办事项 |
| `devlog/CHANGELOG.md` | 更新 | 本文件 |

---

## 九、技术债务 / 已识别风险

1. **中文字体**：ARM 板的 Qt 可能没有中文字体，识别结果中的中文（司令、军长等）可能显示为方框。需要部署中文字体到 `/opt/qt5.6.2/lib/fonts/`
2. **触摸屏设备节点**：假设是 `/dev/input/event0`，实际可能不同，需要 `cat /proc/bus/input/devices` 确认
3. **摄像头设备节点**：假设是 `/dev/video0`，多个 USB 设备时可能变成 `/dev/video1`
4. **屏幕分辨率**：假设 1024×600，如果不是需要在 `run.sh` 中设置 `QT_QPA_FB_SIZE`
5. **V4L2 缓冲区**：固定 4 个内存映射缓冲区，丢 4 帧后取第 5 帧，如果摄像头启动慢可能需要丢更多帧
6. **tslib 校准**：需要先在板子上运行 `ts_calibrate` 生成 `/etc/pointercal`，否则触摸坐标不准
