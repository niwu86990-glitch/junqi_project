#pragma once

#include "result.h"
#include <opencv2/core.hpp>

namespace junqi {

class ColorClassifier {
public:
    PieceColor classify(const cv::Mat& piece_color_roi) const;

private:
    cv::Mat buildHighlightMask(const cv::Mat& hsv) const;
};

} // namespace junqi
