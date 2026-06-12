#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace junqi {

    // 检测图像的背景是浅色还是深色 
inline int detect_thresh_type(const cv::Mat& gray) {
    int bw = std::min(8, std::min(gray.rows, gray.cols) / 20);
    if (bw < 1) bw = 1;

    // 计算边缘的平均亮度
    double m = 0;
    auto mean_roi = [&](int x, int y, int w, int h) {
        m += cv::mean(gray(cv::Rect(x, y, w, h)))[0];
    };
    mean_roi(0, 0, gray.cols, bw);                          // top
    mean_roi(0, gray.rows - bw, gray.cols, bw);             // bottom
    mean_roi(0, bw, bw, gray.rows - 2 * bw);                // left
    mean_roi(gray.cols - bw, bw, bw, gray.rows - 2 * bw);   // right

    double border_mean = m / 4.0;
    return (border_mean < 100) ? cv::THRESH_BINARY : cv::THRESH_BINARY_INV;   //深色返回前者。浅色反之
}

} // namespace junqi
