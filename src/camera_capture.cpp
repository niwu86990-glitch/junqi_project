#include "junqi/camera_capture.h"

#include <opencv2/imgproc.hpp>

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdio>

#include <opencv2/imgcodecs.hpp>

static int snap_count = 0;

namespace junqi {

// ============================================================
//  公开接口
// ============================================================

CameraCapture::~CameraCapture() {
    close();
}

bool CameraCapture::open(const char* device_path, int width, int height) {
    close();  // 先关闭之前可能打开的

    fd_ = ::open(device_path, O_RDWR | O_NONBLOCK, 0);
    if (fd_ < 0) {
        std::fprintf(stderr, "[CameraCapture] 无法打开 %s: %s\n",
                     device_path, std::strerror(errno));
        return false;
    }

    // 查询设备能力
    struct v4l2_capability cap;
    std::memset(&cap, 0, sizeof(cap));
    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        std::fprintf(stderr, "[CameraCapture] VIDIOC_QUERYCAP 失败: %s\n",
                     std::strerror(errno));
        close();
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::fprintf(stderr, "[CameraCapture] %s 不是视频采集设备\n", device_path);
        close();
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        std::fprintf(stderr, "[CameraCapture] %s 不支持流式 I/O\n", device_path);
        close();
        return false;
    }

    // 设置格式：YUYV 4:2:2
    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = static_cast<__u32>(width);
    fmt.fmt.pix.height      = static_cast<__u32>(height);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        // 尝试 MJPEG
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
            std::fprintf(stderr, "[CameraCapture] VIDIOC_S_FMT 失败: %s\n",
                         std::strerror(errno));
            close();
            return false;
        }
        std::fprintf(stderr, "[CameraCapture] 警告: MJPEG 格式暂未完全支持，可能转换失败\n");
    }

    img_width_  = static_cast<int>(fmt.fmt.pix.width);
    img_height_ = static_cast<int>(fmt.fmt.pix.height);

    std::printf("[CameraCapture] 已打开 %s, %dx%d (格式 0x%x)\n",
                device_path, img_width_, img_height_,
                fmt.fmt.pix.pixelformat);

    // 初始化 mmap 缓冲区
    if (!initMmap()) {
        close();
        return false;
    }

    // 启动采集流
    if (!startStreaming()) {
        close();
        return false;
    }

    return true;
}

bool CameraCapture::capture(cv::Mat& frame) {
    if (fd_ < 0) return false;

    struct v4l2_buffer buf_discard;
    memset(&buf_discard, 0, sizeof(buf_discard));
    buf_discard.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_discard.memory = V4L2_MEMORY_MMAP;

    int drop_cnt = 0;
    const int MAX_DROP = 4;  // 最多丢弃全部 4 个缓冲区中的旧帧
    while (drop_cnt < MAX_DROP)
    {
        int ret = ioctl(fd_, VIDIOC_DQBUF, &buf_discard);
        if (ret != 0)
        {
            // 没有就绪帧，队列空了，直接退出丢弃
            break;
        }
    // 取出旧buffer，归还回队列，可被硬件重新写入新帧
    ioctl(fd_, VIDIOC_QBUF, &buf_discard);
    drop_cnt++;
    }

    usleep(100000);

    struct v4l2_buffer buf;
    std::memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // 取出单帧硬件缓冲
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        std::fprintf(stderr, "[CameraCapture] VIDIOC_DQBUF 失败: %s\n",
                     std::strerror(errno));
        return false;
    }
    printf("当前取出buffer索引: %d\n", buf.index);

    // 图像拷贝到应用层cv::Mat（像素数据永久留存，和硬件缓冲无关）
    const auto* data = static_cast<const uint8_t*>(buffers_[buf.index].start);

    yuyvToBgr(data, frame, img_width_, img_height_);

    // 立刻归还硬件缓冲，放回环形队列
    if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
        std::fprintf(stderr, "[CameraCapture] VIDIOC_QBUF 失败: %s\n",
              std::strerror(errno));
        return false;
    }

    char save_path[128];
    snprintf(save_path, sizeof(save_path), "/tmp/snap_%03d.jpg", snap_count++);
    cv::imwrite(save_path, frame);
    printf("[CameraCapture] 单次截图采集成功，已保存至 %s\n", save_path);

    return true;
}

void CameraCapture::close() {
    stopStreaming();
    unmapBuffers();

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool CameraCapture::isOpened() const {
    return fd_ >= 0;
}

// ============================================================
//  V4L2 mmap 初始化
// ============================================================

bool CameraCapture::initMmap() {
    struct v4l2_requestbuffers req;
    std::memset(&req, 0, sizeof(req));
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0) {
        std::fprintf(stderr, "[CameraCapture] VIDIOC_REQBUFS 失败: %s\n",
                     std::strerror(errno));
        return false;
    }

    if (req.count < 2) {
        std::fprintf(stderr, "[CameraCapture] 缓冲区不足 (count=%u)\n", req.count);
        return false;
    }

    buffers_.resize(req.count);

    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            std::fprintf(stderr, "[CameraCapture] VIDIOC_QUERYBUF[%u] 失败: %s\n",
                         i, std::strerror(errno));
            return false;
        }

        buffers_[i].length = buf.length;
        buffers_[i].start  = mmap(nullptr, buf.length,
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd_, buf.m.offset);

        if (buffers_[i].start == MAP_FAILED) {
            std::fprintf(stderr, "[CameraCapture] mmap[%u] 失败: %s\n",
                         i, std::strerror(errno));
            return false;
        }
    }

    return true;
}

void CameraCapture::unmapBuffers() {
    for (auto& b : buffers_) {
        if (b.start) {
            munmap(b.start, b.length);
            b.start  = nullptr;
            b.length = 0;
        }
    }
    buffers_.clear();
}

// ============================================================
//  采集流控制
// ============================================================

bool CameraCapture::startStreaming() {
    // 将所有缓冲区入队
    for (unsigned int i = 0; i < buffers_.size(); ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            std::fprintf(stderr, "[CameraCapture] VIDIOC_QBUF[%u] 失败: %s\n",
                         i, std::strerror(errno));
            return false;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        std::fprintf(stderr, "[CameraCapture] VIDIOC_STREAMON 失败: %s\n",
                     std::strerror(errno));
        return false;
    }

    usleep(120000);

    const int max_retry = 5;
    for (int i = 0; i < 4; ++i)
    {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        int ret = -1;
        int retry_cnt = max_retry;
        while (retry_cnt--)
        {
            ret = ioctl(fd_, VIDIOC_DQBUF, &buf);
            if (ret == 0)
                break;
            // 无数据，短暂等待再试
            if (errno == EAGAIN)
                usleep(25000);
            else
                break;
        }
        if (ret == 0)
        {
            ioctl(fd_, VIDIOC_QBUF, &buf);
        }
    }

    return true;
}

void CameraCapture::stopStreaming() {
    if (fd_ >= 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd_, VIDIOC_STREAMOFF, &type);
    }
}

// ============================================================
//  YUYV → BGR 转换
// ============================================================

void CameraCapture::yuyvToBgr(const uint8_t* yuyv, cv::Mat& bgr,
                               int width, int height) {
    // YUYV 打包为每 2 像素 4 字节: [Y0, U, Y1, V]
    // memcpy 拷贝 mmap 共享缓冲到进程独立内存，规避内核缓冲区覆写
    size_t yuv_bytes = (size_t)width * height * 2;
    cv::Mat yuv_copy(height, width, CV_8UC2);
    memcpy(yuv_copy.data, yuyv, yuv_bytes);

    cv::cvtColor(yuv_copy, bgr, cv::COLOR_YUV2BGR_YUYV);
}

} // namespace junqi
