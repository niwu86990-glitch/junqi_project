# 设计规范文档

## 架构设计

### 模块依赖关系

```
main.cpp
  └── Pipeline (pipeline.h)
        ├── Preprocessor (preprocessor.h)
        ├── Detector (detector.h)
        ├── ColorClassifier (color_classifier.h)
        ├── CharacterExtractor (character_extractor.h)
        └── Recognizer (recognizer.h)
              └── TemplateLibrary (template_library.h)
```

### 模块职责

| 模块 | 职责 | 输入 | 输出 |
|------|------|------|------|
| Preprocessor | 去噪、对比度增强 | BGR 原图 | 灰度图 + 彩色图 |
| Detector | 定位两枚棋子 | 灰度图 + 彩色图 | DetectedPiece 列表 |
| ColorClassifier | 判断棋子颜色 | 棋子彩色 ROI | PieceColor 枚举 |
| CharacterExtractor | 提取并归一化字符 | 棋子灰度 ROI | 64×64 二值图 |
| TemplateLibrary | 加载管理模板 | 模板目录路径 | CharacterTemplate 列表 |
| Recognizer | 模板匹配识别 | 查询字符图 | RecognitionResult |
| Pipeline | 串联全流程 | BGR 原图 | RecognitionOutput |

## 编码规范

### 命名约定
- 类名：PascalCase（如 `ColorClassifier`）
- 方法名：camelCase（如 `buildHighlightMask`）
- 成员变量：snake_case 后缀 `_`（如 `target_size_`）
- 文件名：snake_case（如 `color_classifier.h`）

### 头文件
- 使用 `#pragma once`
- 头文件放在 `include/junqi/` 下
- 公开 API 头文件只包含必要的依赖

### 错误处理
- 空图像：返回空结果，设置 error_message
- 检测失败：返回检测到的部分结果
- 加载失败：返回 false，不抛异常

## 颜色分类设计原理

红色棋子反光是本项目的核心难点。设计策略：

1. **不依赖棋子底色**：棋子底色受光照影响大，不可靠
2. **从字符笔画颜色判断**：笔画区域小、颜色浓，受反光影响相对小
3. **高光像素排除**：HSV 空间中 V>230 且 S<40 的像素视为高光，排除
4. **多色彩空间联合投票**：单一颜色空间在极端光照下可能失效，三空间互补
5. **保守判定**：票数差距需 >1.5 倍才判定，避免误判
