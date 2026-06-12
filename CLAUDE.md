# 军棋棋子图像识别系统 — CLAUDE.md

## 项目概述

C++ + OpenCV 军棋棋子识别系统。识别照片中左右两枚军棋的颜色（红/黑）和中文文字。

## 文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| 需求文档 | [docs/requirements.md](docs/requirements.md) | 功能需求、非功能需求、目标棋子列表 |
| 技术规格 | [docs/technical_spec.md](docs/technical_spec.md) | 技术栈、算法流水线、数据结构、阈值参数 |
| 设计规范 | [docs/design_spec.md](docs/design_spec.md) | 模块架构、编码规范、颜色分类设计原理 |
| 执行指南 | [docs/execution_guide.md](docs/execution_guide.md) | 环境准备、编译、模板采集、评测步骤 |
| 开发日志 | [devlog/CHANGELOG.md](devlog/CHANGELOG.md) | 已完成事项记录 |
| 待办事项 | [devlog/TODO.md](devlog/TODO.md) | 当前待完成任务 |

## 源码索引

| 模块 | 头文件 | 实现文件 |
|------|--------|----------|
| 数据结构 | [include/junqi/result.h](include/junqi/result.h) | — |
| 预处理 | [include/junqi/preprocessor.h](include/junqi/preprocessor.h) | [src/preprocessor.cpp](src/preprocessor.cpp) |
| 棋子检测 | [include/junqi/detector.h](include/junqi/detector.h) | [src/detector.cpp](src/detector.cpp) |
| 颜色分类 | [include/junqi/color_classifier.h](include/junqi/color_classifier.h) | [src/color_classifier.cpp](src/color_classifier.cpp) |
| 字符提取 | [include/junqi/character_extractor.h](include/junqi/character_extractor.h) | [src/character_extractor.cpp](src/character_extractor.cpp) |
| 模板库管理 | [include/junqi/template_library.h](include/junqi/template_library.h) | [src/template_library.cpp](src/template_library.cpp) |
| 识别引擎 | [include/junqi/recognizer.h](include/junqi/recognizer.h) | [src/recognizer.cpp](src/recognizer.cpp) |
| 流程编排 | [include/junqi/pipeline.h](include/junqi/pipeline.h) | [src/pipeline.cpp](src/pipeline.cpp) |
| CLI 入口 | — | [src/main.cpp](src/main.cpp) |

## 资源目录

| 目录 | 用途 |
|------|------|
| [raw_photos/](raw_photos/) | 原始拍摄照片，命名格式 `颜色_棋名_序号.jpg` |
| [templates/](templates/) | 标准字符模板库（64×64 二值 PNG） |
| [templates/metadata.yaml](templates/metadata.yaml) | 字符映射和匹配参数配置 |
| [test_images/](test_images/) | 测试场景照片和标注文件 |

## 构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 关键设计决策

1. **颜色与形状分离**：模板库存字符二值形状（与颜色无关），颜色单独分类
2. **从笔画颜色判红/黑**：避免红色棋子反光导致的颜色误判
3. **多色彩空间投票**：HSV + Lab + YCrCb 三空间联合，1.5 倍差距才判定
4. **TM_CCOEFF_NORMED 为主**：±5° + ±10% 搜索，Hu 矩为回退

## 棋子 ID 对照

```
 1=司令   2=军长   3=师长   4=旅长
 5=团长   6=营长   7=连长   8=排长
 9=工兵  10=地雷  11=炸弹  12=军旗
```
