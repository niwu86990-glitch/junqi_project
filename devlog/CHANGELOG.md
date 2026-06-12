# 开发日志

## 2026-05-13 — 项目初始化

### 完成事项
- 确定算法方案：C++ + OpenCV，模板匹配 + 多色彩空间颜色分类
- 创建项目骨架：目录结构、CMakeLists.txt、数据结构定义
- 实现 6 个核心模块：
  - **预处理**：bilateralFilter 去噪 + CLAHE 对比度增强
  - **棋子检测**：Otsu/自适应二值化 + 轮廓分析 + 几何约束过滤
  - **颜色分类**：HSV + Lab + YCrCb 三空间联合投票，高光像素排除
  - **字符提取**：笔画轮廓合并 + padding + 64×64 归一化
  - **模板库管理**：12 字符目录加载，自动匹配 ID
  - **识别引擎**：TM_CCOEFF_NORMED 互相关 + 多角度/多尺度搜索 + Hu 矩回退
- 实现 CLI 主程序 `junqi_cli`
- 实现辅助工具：`capture_template`（模板采集）、`benchmark`（准确率评测）
- 创建项目文档和开发日志

### 设计决策
- 模板库与颜色无关：只存 12 种字符形状，颜色单独分类
- 从字符笔画颜色判断红/黑，而非棋子底色（解决红色反光问题）
- 使用多色彩空间投票提高颜色分类鲁棒性

## 2026-05-17 — 模板库建立（步骤 1-2）

### 完成事项
- 拍摄 170 张原始棋子照片（12种棋子 × 红黑2色 × 每種6-10张）
- 编译项目（WSL Ubuntu + cmake + OpenCV 4.2）
- 批量提取字符模板：170/170 全部成功
- 自动检测并清理 12 张低质量模板（白色像素 >90%）
- 最终模板库：205 张 64×64 二值 PNG，每种棋子 12-24 张

### 代码修改
- `capture_template`: 添加 `--no-gui` 选项支持批量无头处理
- `capture_template`: 当棋子检测失败时 fallback 使用全图作为 ROI
- `template_library.cpp`: 补充缺少的 `#include <opencv2/imgproc.hpp>`
- 新增调试工具 `debug_detect` 和模板质量检查工具 `check_blank`

## 2026-05-17 — 模板库优化（第二轮）

### 完成事项
- 重写 `capture_template`：添加 deskew（旋转校正）+ 多策略提取（3 种阈值 + top5 轮廓 + deskew/no-deskew）
- 优化评分函数：惩罚白边（border_ratio > 30% 拒绝）、偏好中心分布、收紧 white_ratio 范围（4%-78%）
- 添加 bbox 边缘检测：当字符框触及棋子边缘时，自动收紧轮廓过滤阈值
- 新增 `score_templates` 工具：对模板排序打分，支持 `--delete` 删除低分模板
- 精简模板库：每种棋子保留评分最高的 8 张，共计 96 张，全部通过质量检查

### 关键修复
- `recognizer.cpp`: 修复 `match_ccoeff` 在模板小于查询时只取 (0,0) 而非 maxVal 的 bug；补齐 padding 逻辑
- `character_extractor.cpp`: 大 ROI 先找棋子轮廓再提取字符；降低轮廓过滤阈值（3%→1.5%）
- `detector.cpp`: 放宽面积/长宽比约束；添加中线分割 fallback
- `CMakeLists.txt`: 新增 `debug_detect`、`debug_pipeline`、`check_blank`、`score_templates` 目标

### 待解决
- 场景识别（benchmark）字符准确率仍低，需从场景照片处理流程入手

## 2026-06-07 — ARM 交叉编译 & Qt GUI 部署

### 完成事项
- 新增 4 个模块：`BattleJudge`（对战判定）、`CameraCapture`（V4L2 直驱）、`ProcessingWorker`（工作线程）、`MainWindow`（Qt GUI）
- C++17 → C++11 降级：`std::filesystem` → POSIX，`std::make_unique` → `new`，适配 GCC 4.8.3
- V4L2 直驱摄像头（YUYV 640×480 → BGR），不再依赖 `opencv_videoio`
- 配置 ARM 交叉编译工具链（Sourcery CodeBench Lite 2014.05，目标 S5PV210 Cortex-A8）
- ARM 交叉编译通过：`junqi_gui` (206KB) + `junqi_cli` (143KB) + `libjunqi_core.a` (200KB)
- 生成完整部署包 `deploy/junqi_deploy.tar.gz` (24MB)，含 Qt5/OpenCV/tslib 所有 .so 及启动脚本
- 统一 tslib 路径到 `/home/cheese/x210/tslib`，删除重复副本

### 详细日志
见 [devlog/2026-06-07-arm-cross-compile.md](2026-06-07-arm-cross-compile.md)

### 待解决
- 开发板 GUI 显示 Segfault（详见 [2026-06-07-gui-debug.md](2026-06-07-gui-debug.md)）
- 开发板实测：摄像头拍照、触摸屏交互、识别准确率
- 触摸屏校准

## 2026-06-07 — GUI 部署调试（4 轮迭代）+ 完整技术文档

详见 [devlog/2026-06-07-gui-debug.md](2026-06-07-gui-debug.md)，文档包含：

- **Qt GUI 源码框架**：线程模型、类图、信号槽流程、每个 .h/.cpp 的职责说明
- **ARM 交叉编译体系**：工具链配置、CMakeLists.txt 交叉编译段、C++17→C++11 降级适配
- **项目文件清单**：每文件的用途标注 + 保留/可删/仅桌面分类 + 轻量化清理命令
- **4 轮调试迭代**：tslib→字体→CSS→最小化 UI，逐步排除崩溃原因
- **诊断结果**：framebuffer 1024×1200 双倍高度、offscreen 可用、7 项因素已排除

## 2026-06-10 — B、C 同学代码合并

### B 的改动（CMakeLists.txt + 部署脚本）

- **CMakeLists.txt**：改为 ARM 硬编码路径（Qt5.6.2 `/home/lwx/...`、OpenCV `/home/lwx/...`）、注释掉桌面 tools、添加 tslib 链接
- **deploy/pack.sh**：新增快速编译打包脚本，路径改为 `/home/lwx/`
- **deploy/package.sh**：完整部署打包脚本，含 Qt5/OpenCV/tslib 全部 .so
- **deploy/run.sh**（旧版）：硬编码 `/root/junqi` 路径的启动脚本

### C 的改动（camera_capture.cpp + deploy 脚本优化）

- **src/camera_capture.cpp**（核心 Bug 修复）：
  1. `yuyvToBgr()` 内增加 `memcpy` 把 mmap 共享 YUYV 数据拷贝到进程独立内存，规避内核缓冲区覆写
  2. 删除 `capture()` 末尾的 `usleep(50000)` 阻塞延时，解决帧队列堆积
  3. 新增最多丢弃 5 帧旧帧的限流逻辑（`MAX_DROP=5`），避免无限 DQBUF 掏空缓冲区
  4. 删除重复的 memory 赋值冗余代码，完善错误日志打印
- **deploy/run.sh**（改进版）：`APP_ROOT` 动态定位替代硬编码路径、补全 OpenCV 库搜索路径、加回 Qt fb 配置变量
- **deploy/pack.sh**（优化版）：新增 `junqi_cli` 编译、`bin/` 子目录结构、`strip` 瘦身、`gzip -9` 最高压缩

## 2026-06-12 — 项目审查与整理 (cheese)

### 审查发现

- **CMakeLists.txt**：B/C 版本中 ARM 交叉编译路径（`/home/lwx/`）与 cheese 本机不兼容
- **缺失文件**：`tools/`、`docs/`、`devlog/`、`CLAUDE.md` 未随项目同步
- **残留文件**：`build_arm/` 含 lwx 机器缓存、`deploy/junqi_deploy/` 含 17MB ARM 部署包、`root/junqi/run.sh` 为旧版启动脚本、`CMakeLists.txt:Zone.Identifier` Windows 标记文件

### 修复操作

- 重写 `CMakeLists.txt`：桌面编译使用 `find_package(OpenCV)` 系统库，ARM 交叉编译通过 toolchain 文件配置，Qt5 GUI 改为可选 (`find_package(Qt5 QUIET)`)
- 恢复 `tools/` 目录（从原项目复制）
- 恢复 `docs/`、`devlog/`、`CLAUDE.md`（从原项目复制）
- 清理残留：删除 `build_arm/`、`deploy/junqi_deploy/`、`deploy/junqi_deploy.tar.gz`、`root/`、`CMakeLists.txt:Zone.Identifier`
- 修复 tools 编译：设置 `cxx_std_17`（benchmark/capture_template/debug_pipeline/score_templates 需要 C++17）
- 新建 `README.md`：项目总览、编译指南、结构说明

### 桌面编译验证

```
✅ libjunqi_core.a  — 核心算法静态库
✅ junqi_cli        — CLI 命令行工具
✅ benchmark        — 准确率评测工具
✅ capture_template — 模板采集工具
✅ debug_pipeline   — 流水线调试工具
✅ score_templates  — 模板质量评分工具
❌ junqi_gui        — Qt5 未安装，桌面跳过（ARM 交叉编译正常）
```

## 2026-06-12 — 单棋子双阶段识别流程

### 功能调整

- 原先一次截图识别左右两枚棋子，改为红方和黑方分别点击按钮、分别截图识别一枚棋子
- 每方识别后通过私密弹窗显示颜色、棋名和置信度
- 支持确认识别结果，或从 12 种棋子中人工纠正错误结果
- 确认后主界面隐藏棋名，只显示该方已完成，防止对手看到结果
- 双方均确认后才调用 `BattleJudge` 显示红方胜、黑方胜或平局
- 增加“开始下一轮”按钮，清除双方结果并恢复识别状态

### 部署调整

- `deploy/package.sh` 和 `deploy/pack.sh` 改为自动使用当前仓库路径，避免打包旧目录中的二进制
- 两种打包脚本成功后自动将部署包复制到 Windows 桌面，桌面不可用时明确报错
- `docs/execution_guide.md` 增加 X210 的 TFTP 下载、解压覆盖和启动指令

### X210 显示调整

- 启动 Qt GUI 前清除 `/dev/tty0` 文字和 `/dev/fb0` 上一帧画面
- 主窗口改为无边框 `1024×600`，从屏幕左上角开始覆盖 X210 物理 LCD
- 保留普通 `show()`，避免 `showFullScreen()` 在 linuxfb 上触发模式切换问题
- 识别确认、人工纠错、错误提示和胜负结果弹窗增加纯色背景与实线边框
- 主界面重新排版为标题、双识别按钮、双方状态卡片和中下方大号判定结果
- 红方胜使用红色结果文字，黑方胜使用黑色，平局使用绿色，并同步应用于结果弹窗

## 2026-06-12 — 识别算法鲁棒性增强

- 棋子检测改为原始灰度、光照归一化灰度、双极性阈值、局部阈值和边缘候选联合评分
- 不再依赖图像边缘亮度判断背景必须为黑色或白色
- 字符提取同时生成局部自适应、黑帽暗笔画、红色色差和组合候选
- 修复封闭字符轮廓被填满的问题，保留“团”等字内部笔画
- 模板匹配改为笔画重叠、双向轮廓距离和相关性联合评分
- 缓存模板旋转/尺度版本与距离图，控制 X210 上的重复计算开销
- 新增 `robustness_test`，覆盖黑/亮/纹理背景、红黑字、旋转、位置、光照渐变和高光
- 合成测试从旧算法字符 35.4%、颜色 75% 提升至字符 85.4%、颜色 100%
- 支持通过 `JUNQI_SAVE_LAST_CAPTURE` 可选保存最近一次开发板原始截图

### 红色反光专项优化

- 红色判断从固定 HSV/Lab/Cr 阈值改为相对棋子底色的色度偏移
- 综合 `R-max(G,B)`、Lab `a*` 和 YCrCb `Cr`，至少两种色彩描述一致才形成红色种子
- 高光像素不再全部丢弃；仅当周围存在红色笔画时恢复高光中心
- 黑色判断增加低红色度约束，避免暗红色笔画被归为黑色
- 新增浅粉、低饱和、暗光和宽高光红字场景，合成回归扩展至 84 组，颜色准确率 100%
- 流水线计时改为 `steady_clock`，避免系统时钟变化产生负耗时

## 2026-06-12 — 已隐藏结果可再次查看

- 双方状态卡片改为可点击按钮，未完成识别时保持禁用
- 棋子确认后显示“结果已隐藏，点击查看”，避免主界面直接泄露棋子内容
- 从确认识别到开始下一轮之前，均可点击对应按钮重新查看已保存的识别结果
- 再次查看弹窗仅提供“确认”按钮，关闭后结果重新隐藏，不影响已保存结果和对战判定
