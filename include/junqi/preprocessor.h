#pragma once

#include <opencv2/core.hpp>
#include <utility>

namespace junqi {

struct PreprocessedImage {
    cv::Mat gray;
    cv::Mat color;
};

class Preprocessor {
public:
    PreprocessedImage process(const cv::Mat& input) const;
};

} // namespace junqi
