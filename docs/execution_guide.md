# 执行步骤指南

## 环境准备

### 1. 安装依赖 (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake libopencv-dev
```

### 2. 编译项目
```bash
cd junqi
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 第一阶段：建立标准模板库

### 步骤 1：拍摄原始照片
- 对每种棋子（共 12 种），拍摄 6-10 张照片
- 拍摄条件：尽量正面俯拍，光照均匀
- 每次拍摄轻微变化棋子角度（旋转 1-3 度）和位置
- 命名格式：`颜色_棋子名_序号.jpg`
  - 示例：`红_司令_01.jpg`、`黑_军长_03.jpg`
- 放入 `raw_photos/` 目录

### 步骤 2：提取字符模板
```bash
# 对每张原始照片运行
./build/capture_template raw_photos/红_司令_01.jpg 1
# char_id: 1=司令 2=军长 3=师长 4=旅长 5=团长 6=营长
#           7=连长 8=排长 9=工兵 10=地雷 11=炸弹 12=军旗

# 模板会自动保存到 templates/01_siling/sample_001.png 等位置
```

### 步骤 3：验证模板质量
```bash
# 确认每种棋子至少有 6 张模板
ls templates/*/sample_*.png | wc -l
```

## 第二阶段：准备测试数据

### 步骤 4：拍摄测试场景
- 将两枚棋子（一红一黑）放一起拍照
- 命名格式：`scene_001.jpg`、`scene_002.jpg` …
- 放入 `test_images/labeled/` 目录

### 步骤 5：编写标注文件
- 每张测试照片配一个同名 `.yaml` 文件
- 格式：
```yaml
left:
  color: red
  character: "司令"
right:
  color: black
  character: "军长"
```

## 第三阶段：评测与调优

### 步骤 6：运行基准测试
```bash
./build/benchmark test_images/labeled/ templates/
```

### 步骤 7：分析结果
- 查看颜色准确率和字符准确率
- 查看混淆矩阵，找出容易混淆的字符对
- 对误判样本做人工分析

### 步骤 8：参数调优
- 颜色分类：调整 `color_classifier.cpp` 中的阈值
- 模板匹配：调整 `templates/metadata.yaml` 中的 matching 参数
- 重新编译并评测，迭代直到达标

## 第四阶段：日常使用

```bash
# 识别单张照片
./build/junqi_cli photo.jpg templates/
```
