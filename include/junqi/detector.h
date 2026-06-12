#pragma once

#include <opencv2/core.hpp>
#include <vector>

namespace junqi {

struct DetectedPiece {
    cv::Rect bounding_box;
    cv::RotatedRect rotated_box;
    cv::Mat roi_color;
    cv::Mat roi_gray;
    bool is_left = false;
};

class Detector {
public:
    std::vector<DetectedPiece> detect(const cv::Mat& gray, const cv::Mat& color) const;
};

} // namespace junqi
