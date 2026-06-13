# 技术规格文档

## 技术栈

| 项目 | 选型 |
|------|------|
| 语言 | C++17 |
| 图像库 | OpenCV 4.x (core, imgproc, imgcodecs, highgui) |
| 构建系统 | CMake 3.14+ |
| 编译器 | GCC 9+ / Clang 10+ |
| 平台 | Linux x86_64（后续 ARM 交叉编译） |

## 算法流水线

```
原始图像 → 预处理 → 棋子检测 → 棋子校正 → 字符提取 → 模板匹配 → 识别结果
```

### 1. 预处理
- `cv::bilateralFilter`: d=5, sigmaColor=50, sigmaSpace=50 — 保边去噪
- `cv::createCLAHE`: clipLimit=3.0, tileSize=(25,25) — 局部对比度增强

### 2. 棋子检测
- 主方案：Otsu 二值化 + 形态学闭运算 + 轮廓检测
- 备选：自适应阈值（Otsu 白像素比不在 5%-50% 时启用）
- 几何约束：面积在图像 2%-60% 之间，长宽比 1.1-1.9
- 排序：按 x 坐标区分左右

### 3. 字符提取
- 棋子 ROI 内 Otsu 二值化
- 所有笔画轮廓合并 bounding box
- 四周加 10% padding
- 归一化到 64×64

### 4. 模板匹配
- 主方法：`cv::TM_CCOEFF_NORMED`
- 搜索空间：旋转 [-5°,+5°] 步长 1°，缩放 [0.90,1.10] 步长 0.05
- 回退：Hu 矩 `cv::matchShapes`（主方法置信度 <0.80 时启用）

## 数据结构

```cpp
PieceResult { character, character_id, confidence, bounding_box }
RecognitionOutput { left_piece, right_piece, success, error_message, elapsed_ms }
```

红黑阵营不属于图像识别结果，由 GUI 的红方/黑方操作入口确定。

## 模板库规格

- 格式：64×64 二值 PNG
- 数量：12 种 × 6-10 样本
- 目录结构：`templates/XX_棋名/sample_NNN.png`
- 元数据：`templates/metadata.yaml`
