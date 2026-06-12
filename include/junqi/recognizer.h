#pragma once

#include "template_library.h"
#include <opencv2/core.hpp>
#include <utility>
#include <vector>

namespace junqi {

struct RecognitionResult {
    int character_id = -1;
    std::string character_name;
    float confidence = 0.0f;
    float margin = 0.0f;
};

struct RecognizerConfig {
    double rot_start = -8.0;
    double rot_end = 8.0;
    double rot_step = 4.0;
    double scale_start = 0.90;
    double scale_end = 1.10;
    double scale_step = 0.10;
    float confidence_threshold = 0.36f;
};

class Recognizer {
public:
    explicit Recognizer(const TemplateLibrary& lib,
                        const RecognizerConfig& cfg = RecognizerConfig{});

    RecognitionResult recognize(const cv::Mat& piece_roi) const;

private:
    struct CachedVariant {
        cv::Mat binary;
        cv::Mat distance_to_stroke;
        int stroke_pixels = 0;
    };

    struct CachedCharacter {
        int character_id = -1;
        std::string character_name;
        std::vector<CachedVariant> variants;
    };

    const TemplateLibrary& library_;
    RecognizerConfig config_;
    mutable bool cache_ready_ = false;
    mutable std::vector<CachedCharacter> cache_;

    cv::Mat transform_image(const cv::Mat& src, double angle,
                            double scale) const;
    void build_cache() const;
    float shape_score(const cv::Mat& scene,
                      const cv::Mat& scene_distance,
                      int scene_pixels,
                      const CachedVariant& sample) const;
};

} // namespace junqi
