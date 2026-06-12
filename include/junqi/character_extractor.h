#pragma once

#include <opencv2/core.hpp>
#include <vector>

namespace junqi {

class CharacterExtractor {
public:
    cv::Mat extract(const cv::Mat& piece_gray) const;
    std::vector<cv::Mat> extract_candidates(const cv::Mat& piece_color,
                                            const cv::Mat& piece_gray) const;
    cv::Mat normalize(const cv::Mat& char_binary, cv::Size target_size) const;

private:
    cv::Mat clean_and_crop(const cv::Mat& mask) const;
};

} // namespace junqi
