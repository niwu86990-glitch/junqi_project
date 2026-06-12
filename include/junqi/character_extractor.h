#pragma once

#include <opencv2/core.hpp>

namespace junqi {

class CharacterExtractor {
public:
    cv::Mat extract(const cv::Mat& piece_gray) const;
    cv::Mat normalize(const cv::Mat& char_binary, cv::Size target_size) const;
};

} // namespace junqi
