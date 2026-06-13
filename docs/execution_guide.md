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
  character: "司令"
right:
  character: "军长"
```

## 第三阶段：评测与调优

### 步骤 6：运行基准测试
```bash
./build/benchmark test_images/labeled/ templates/
```

### 步骤 7：分析结果
- 查看字符准确率
- 查看混淆矩阵，找出容易混淆的字符对
- 对误判样本做人工分析

### 步骤 8：参数调优
- 模板匹配：调整 `templates/metadata.yaml` 中的 matching 参数
- 重新编译并评测，迭代直到达标

## 第四阶段：日常使用

```bash
# 识别单张照片
./build/junqi_cli photo.jpg templates/
```

## 第五阶段：X210 开发板部署

### 交叉编译

```bash
cmake -S . -B build_arm \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain_study210.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_QT_GUI=ON
cmake --build build_arm --target junqi_gui junqi_cli -j"$(nproc)"
```

### 生成完整部署包

```bash
bash deploy/package.sh
```

生成文件为 `deploy/junqi_deploy.tar.gz`。将它放入地址为
`192.168.1.30` 的 TFTP 服务器根目录。脚本还会自动将同名部署包复制到
Windows 桌面，例如 `/mnt/c/Users/cheese/Desktop/junqi_deploy.tar.gz`。

### 开发板安装和启动

在开发板 `/root` 目录执行：

```sh
cd /root
tftp -g -r junqi_deploy.tar.gz 192.168.1.30
gunzip junqi_deploy.tar.gz
cd / && tar xf /root/junqi_deploy.tar -C /
sh /opt/junqi/run.sh
```

重新部署前可删除开发板中残留的同名压缩包，避免 `tftp` 或 `gunzip`
因目标文件已存在而失败。

### 开发板截图

每次识别会保存 `/tmp/snap_XXX.jpg`。程序正常退出、收到常见终止信号或下次
启动时会自动删除这些截图；突然断电时的残留将在下次启动时清理。

### 显示异常排查

若 LCD 出现开发板原有的 `Poweroff Test` 画面，但串口没有显示项目退出，说明
`qttest` 或 `hdmi_x210` 正在与项目争用 `/dev/fb0`。正常启动日志应包含：

```text
=== DIAG: board display services ===
pausing pid=... command=qttest
pausing pid=... command=hdmi_x210
```

不要直接杀死 `hdmi_x210`。该进程可能参与维持显示硬件状态，终止后会导致
Qt 5.6 在 `show()` 阶段段错误，日志表现为：

```text
Segmentation fault
[RUN] junqi_gui exited with status 139
```

当前启动脚本使用 `SIGSTOP` 暂停板载界面，项目退出时再通过 `SIGCONT` 恢复。
同时应保留以下已验证配置：

```sh
QT_QPA_PLATFORM=linuxfb
QT_QPA_FB=/dev/fb0
QT_QPA_LINUXFB_NO_DOUBLE_BUFFER=1
```

不要在未单独验证时改成 `linuxfb:fb=/dev/fb0:size=1024x600`，也不要在缺少
`TSLIB_CONFFILE` 等配置时强制设置 `QT_QPA_FB_TSLIB=1`。
