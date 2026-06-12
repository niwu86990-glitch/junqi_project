#pragma once

#include "template_library.h"
#include <opencv2/core.hpp>
#include <utility>

namespace junqi {

struct RecognitionResult {
    int character_id = -1;
    std::string character_name;
    float confidence = 0.0f;
};

struct RecognizerConfig {
    double rot_start = -3.0;
    double rot_end = 3.0;
    double rot_step = 1.0;
    double scale_start = 0.90;
    double scale_end = 1.10;
    double scale_step = 0.05;
    float confidence_threshold = 0.40f;
};

class Recognizer {
public:
    explicit Recognizer(const TemplateLibrary& lib,
                        const RecognizerConfig& cfg = RecognizerConfig{});

    RecognitionResult recognize(const cv::Mat& piece_roi) const;

private:
    const TemplateLibrary& library_;
    RecognizerConfig config_;

    cv::Mat rotate_image(const cv::Mat& src, double angle) const;
};

} // namespace junqi
