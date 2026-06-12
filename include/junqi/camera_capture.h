#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace junqi {

/// 基于 V4L2 的摄像头采集封装（无需 libopencv_videoio）
///
/// 直接使用 Linux V4L2 API + mmap 采集帧，避免对 OpenCV videoio 模块的依赖。
/// 嵌入式 ARM Linux 上 USB 摄像头通常支持 YUYV 格式。
class CameraCapture {
public:
    CameraCapture() = default;
    ~CameraCapture();

    // 禁止拷贝
    CameraCapture(const CameraCapture&) = delete;
    CameraCapture& operator=(const CameraCapture&) = delete;

    /// 打开摄像头设备
    /// @param device_path  设备路径，如 "/dev/video0"
    /// @param width        采集分辨率宽
    /// @param height       采集分辨率高
    bool open(const char* device_path = "/dev/video0",
              int width = 640, int height = 480);

    /// 采集一帧（内部丢前几帧以稳定曝光，返回稳定帧）
    bool capture(cv::Mat& frame);

    /// 关闭摄像头
    void close();

    /// 是否已打开
    bool isOpened() const;

private:
    // V4L2 内部实现
    bool initMmap();
    bool startStreaming();
    void stopStreaming();
    void unmapBuffers();

    // YUYV → BGR 转换
    void yuyvToBgr(const uint8_t* yuyv, cv::Mat& bgr, int width, int height);

    int fd_ = -1;

    struct Buffer {
        void*  start = nullptr;
        size_t length = 0;
    };
    std::vector<Buffer> buffers_;
    int img_width_  = 640;
    int img_height_ = 480;
};

} // namespace junqi
