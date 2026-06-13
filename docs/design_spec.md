# 设计规范文档

## 架构设计

### 模块依赖关系

```
main.cpp
  └── Pipeline (pipeline.h)
        ├── Preprocessor (preprocessor.h)
        ├── Detector (detector.h)
        ├── CharacterExtractor (character_extractor.h)
        └── Recognizer (recognizer.h)
              └── TemplateLibrary (template_library.h)
```

### 模块职责

| 模块 | 职责 | 输入 | 输出 |
|------|------|------|------|
| Preprocessor | 去噪、对比度增强 | BGR 原图 | 灰度图 + 彩色图 |
| Detector | 定位两枚棋子 | 灰度图 + 彩色图 | DetectedPiece 列表 |
| CharacterExtractor | 提取并归一化字符 | 棋子灰度 ROI | 64×64 二值图 |
| TemplateLibrary | 加载管理模板 | 模板目录路径 | CharacterTemplate 列表 |
| Recognizer | 模板匹配识别 | 查询字符图 | RecognitionResult |
| Pipeline | 串联全流程 | BGR 原图 | RecognitionOutput |

## 编码规范

### 命名约定
- 类名：PascalCase（如 `CharacterExtractor`）
- 方法名：camelCase（如 `extractCandidates`）
- 成员变量：snake_case 后缀 `_`（如 `target_size_`）
- 文件名：snake_case（如 `character_extractor.h`）

### 头文件
- 使用 `#pragma once`
- 头文件放在 `include/junqi/` 下
- 公开 API 头文件只包含必要的依赖

### 错误处理
- 空图像：返回空结果，设置 error_message
- 检测失败：返回检测到的部分结果
- 加载失败：返回 false，不抛异常

## 阵营归属

GUI 已经将红方和黑方拆成独立识别入口，因此阵营由用户选择的入口确定，
不再从图像中分类。彩色 ROI 仍用于提取红色或黑色字符笔画。
