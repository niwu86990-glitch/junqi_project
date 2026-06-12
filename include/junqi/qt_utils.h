#pragma once

#include <QImage>
#include <opencv2/core.hpp>

/// QImage ↔ cv::Mat 转换工具
/// 从现有 qt-opencv 示例中提取，经适配后复用
namespace junqi {

/// QImage → cv::Mat（4 通道 BGRA）
inline cv::Mat qimage_to_mat(const QImage& qimage) {
    cv::Mat mat(qimage.height(), qimage.width(), CV_8UC4,
                const_cast<uchar*>(qimage.bits()),
                static_cast<size_t>(qimage.bytesPerLine()));
    cv::Mat mat2(mat.rows, mat.cols, CV_8UC4);
    int from_to[] = { 0, 0, 1, 1, 2, 2, 3, 3 };
    cv::mixChannels(&mat, 1, &mat2, 1, from_to, 4);
    return mat2;
}

/// cv::Mat → QImage（深拷贝版本，线程安全）
inline QImage mat_to_qimage(const cv::Mat& mat) {
    if (mat.empty()) return {};

    if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage img(rgb.cols, rgb.rows, QImage::Format_RGB888);
        for (int i = 0; i < rgb.rows; ++i) {
            memcpy(img.scanLine(i), rgb.ptr(i),
                   static_cast<size_t>(img.bytesPerLine()));
        }
        return img;
    }

    if (mat.type() == CV_8U) {
        QImage img(mat.cols, mat.rows, QImage::Format_Grayscale8);
        for (int i = 0; i < mat.rows; ++i) {
            memcpy(img.scanLine(i), mat.ptr(i),
                   static_cast<size_t>(img.bytesPerLine()));
        }
        return img;
    }

    return {};
}

} // namespace junqi
