# 待办事项

## 当前状态 (2026-06-12)

- ✅ 桌面编译通过：核心库 + CLI + 4 个开发工具
- ✅ 项目结构审查完成，残留文件已清理
- ✅ 项目文档已更新（README.md + CHANGELOG.md）
- ✅ B/C 同学改动已合并审查
- ⚠️ ARM 交叉编译：toolchain 文件需更新为本机路径
- ⚠️ GUI 调试：开发板 linuxfb Segfault 未解决

---

## 一、代码质量检查（下一步）

### 1.1 静态检查
- [ ] 检查所有 `#include` 是否正确且无冗余
- [ ] 检查 C++11 兼容性（ARM GCC 4.8.3 不支持 C++14/17）
  - 确认 `template_library.cpp` POSIX 化完整
  - 确认 `processing_worker.cpp` 无 `std::make_unique`
- [ ] 检查内存管理：new/delete 配对、智能指针使用
- [ ] 检查 V4L2 资源释放：STREAMOFF → munmap → close 是否在任何错误路径都执行

### 1.2 编译警告
- [ ] 桌面编译 `-Wall -Wextra`，修复所有警告
- [ ] ARM 交叉编译 GCC 4.8.3 警告检查

### 1.3 代码审查重点
- [ ] `camera_capture.cpp`：C 的修改是否引入新问题
  - memcpy 拷贝是否可能 OOM（YUYV 640×480×2 = 600KB，安全）
  - MAX_DROP=5 限流是否合理（队列中只有 4 个 buffer，最多丢弃 4 个）
  - startStreaming() 中的 usleep(120000) + DQBUF 循环逻辑是否健壮
- [ ] `pipeline.cpp`：`pieces.size() < 2` 时仍继续处理棋子（逻辑 bug）
- [ ] `template_library.cpp`：POSIX 目录遍历是否有资源泄漏

---

## 二、ARM 交叉编译环境修复

### 2.1 工具链配置
- [ ] 更新 `cmake/toolchain_study210.cmake`：路径改为本机实际路径
- [ ] 更新 `arm-gcc.cmake`：路径改为本机实际路径
- [ ] 更新 `build.sh`：路径从 `/home/ten/` 改为本机实际路径

### 2.2 ARM 编译验证
- [ ] 验证 `junqi_gui` 交叉编译通过
- [ ] 验证 `junqi_cli` 交叉编译通过
- [ ] 确认 strip 后体积：GUI < 200KB, CLI < 150KB

---

## 三、部署脚本适配

### 3.1 pack.sh（快速打包）
- [ ] 更新 `BUILD_DIR` 为本机路径
- [ ] 更新 `run.sh` 来源路径
- [ ] 更新 Windows 输出路径
- [ ] 更新 strip 工具前缀

### 3.2 package.sh（完整部署包）
- [ ] 更新所有 `/home/lwx/` 路径
- [ ] 更新 Qt5/OpenCV/tslib 库源路径
- [ ] 确认打包后目录结构正确

### 3.3 run.sh（启动脚本）
- [ ] 确认 `APP_ROOT` 动态定位正确
- [ ] 确认库路径（LD_LIBRARY_PATH）与开发板实际路径一致
- [ ] 确认 Qt fb 配置变量完整

---

## 四、开发板部署测试

### 4.1 传输与解压
- [ ] 用 Tftpd64 传部署包到开发板 (IP: 192.168.1.20)
- [ ] 开发板解压：`gunzip xxx.tar.gz && tar xf xxx.tar -C /`

### 4.2 GUI 崩溃调试（linuxfb Segfault）
- [ ] 测试 offscreen 平台：`QT_QPA_PLATFORM=offscreen ./junqi_gui`
- [ ] 如 offscreen 正常 → linuxfb 插件与 S5PV210 fb 驱动不兼容
  - [ ] 尝试 `QT_QPA_FB_SIZE=1024x600`
  - [ ] 尝试 `fbset -xres 1024 -yres 600 -vxres 1024 -vyres 600`
  - [ ] 尝试最简单的空 QLabel 窗口测试 linuxfb
- [ ] 如 offscreen 也崩溃 → Qt 核心库问题
  - [ ] 检查 Qt 5.6.2 编译选项
  - [ ] 尝试 `LD_DEBUG=all` 跟踪动态链接

### 4.3 功能验证
- [ ] 摄像头拍照：V4L2 采集 + YUYV→BGR 转换
- [ ] 触摸屏：ts_calibrate 校准 + ts_test 验证
- [ ] 端到端识别：放两枚棋子 → 拍照 → 看识别结果
- [ ] 判定规则验证：测试几种典型对战组合

---

## 五、算法改进（项目收尾）

### 5.1 识别准确率
- [ ] 分析 benchmark 结果，定位识别失败的模式
- [ ] 光照不变性改进：多尺度 Retinex / 伽马校正
- [ ] 模板匹配优化：多角度 ±10°、多尺度 ±15%
- [ ] 颜色分类鲁棒性：减少红色棋子反光误判

### 5.2 性能优化
- [ ] 桌面端目标：单张处理 < 500ms
- [ ] ARM 端目标：单张处理 < 2s
- [ ] 模板匹配加速：金字塔搜索策略
- [ ] 减少 cv::cvtColor 调用次数

### 5.3 鲁棒性
- [ ] 不均匀光照下的棋子检测
- [ ] 棋子部分遮挡的容错
- [ ] 摄像头自动曝光稳定后的最佳拍照时机

---

## 六、文档完善

- [ ] 更新 `docs/execution_guide.md` 加入 ARM 部署步骤
- [ ] 创建 `devlog/2026-06-12-project-review.md` 记录本次审查
- [ ] 更新 `CLAUDE.md` 反映文件结构变化

---

## 附录：开发板快速参考

| 参数 | 值 |
|------|-----|
| IP 地址 | 192.168.1.20 |
| 开发机 IP | 192.168.1.30 |
| 程序目录 | `/root/junqi/` |
| tftp 下载 | `tftp -g -r <file> 192.168.1.30` |
| 解压命令 | `gunzip xxx.tar.gz && tar xf xxx.tar -C /` |
| LCD 分辨率 | 1024×600（fb 虚拟 1024×1200） |
| 触摸设备 | `/dev/input/event2` |
| 摄像头 | `/dev/video0` |
| Qt lib 目录 | `/root/qt5.6.2/lib/` |
| OpenCV lib 目录 | `/root/opencv3/lib/` |
| tslib 目录 | `/root/tslib/` |
