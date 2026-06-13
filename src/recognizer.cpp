#include "junqi/recognizer.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace junqi {

Recognizer::Recognizer(const TemplateLibrary& lib, const RecognizerConfig& cfg)
    : library_(lib), config_(cfg) {}

cv::Mat Recognizer::transform_image(const cv::Mat& src, double angle,
                                    double scale) const {
    cv::Point2f center(src.cols / 2.0f, src.rows / 2.0f);
    cv::Mat transform = cv::getRotationMatrix2D(center, angle, scale);
    cv::Mat dst;
    cv::warpAffine(src, dst, transform, src.size(), cv::INTER_NEAREST,
                   cv::BORDER_CONSTANT, cv::Scalar(0));
    cv::threshold(dst, dst, 128, 255, cv::THRESH_BINARY);
    return dst;
}

void Recognizer::build_cache() const {
    if (cache_ready_) return;
    cache_.clear();

    for (const auto& tmpl : library_.templates()) {
        CachedCharacter character;
        character.character_id = tmpl.character_id;
        character.character_name = tmpl.chinese_name;

        for (const auto& sample_source : tmpl.samples) {
            cv::Mat sample;
            cv::threshold(sample_source, sample, 128, 255,
                          cv::THRESH_BINARY);
            if (sample.size() != cv::Size(64, 64)) {
                cv::resize(sample, sample, cv::Size(64, 64), 0, 0,
                           cv::INTER_NEAREST);
            }

            for (double angle = config_.rot_start;
                 angle <= config_.rot_end + 0.001;
                 angle += config_.rot_step) {
                for (double scale = config_.scale_start;
                     scale <= config_.scale_end + 0.001;
                     scale += config_.scale_step) {
                    CachedVariant variant;
                    variant.binary =
                        transform_image(sample, angle, scale);
                    variant.stroke_pixels =
                        cv::countNonZero(variant.binary);
                    if (variant.stroke_pixels == 0) continue;

                    cv::Mat inverse;
                    cv::bitwise_not(variant.binary, inverse);
                    cv::distanceTransform(inverse,
                                          variant.distance_to_stroke,
                                          cv::DIST_L2, 3);
                    character.variants.push_back(variant);
                }
            }
        }
        cache_.push_back(character);
    }
    cache_ready_ = true;
}

float Recognizer::shape_score(const cv::Mat& scene,
                              const cv::Mat& scene_distance,
                              int scene_pixels,
                              const CachedVariant& sample) const {
    if (scene_pixels == 0 || sample.stroke_pixels == 0) return 0.0f;

    cv::Mat overlap;
    cv::bitwise_and(scene, sample.binary, overlap);
    const int overlap_pixels = cv::countNonZero(overlap);
    const float dice =
        2.0f * overlap_pixels /
        static_cast<float>(scene_pixels + sample.stroke_pixels);

    const double scene_to_sample =
        cv::mean(sample.distance_to_stroke, scene)[0];
    const double sample_to_scene =
        cv::mean(scene_distance, sample.binary)[0];
    const double mean_distance =
        (scene_to_sample + sample_to_scene) * 0.5;
    const float chamfer =
        static_cast<float>(std::exp(-mean_distance / 3.5));

    // For equal-size binary images, TM_CCOEFF_NORMED can be calculated
    // exactly from the foreground counts and their overlap. Avoiding thousands
    // of tiny matchTemplate calls is significant on the X210 Cortex-A8.
    const double pixels = static_cast<double>(scene.total());
    const double correlation_denominator = std::sqrt(
        static_cast<double>(scene_pixels) *
        (pixels - scene_pixels) *
        sample.stroke_pixels *
        (pixels - sample.stroke_pixels));
    float correlation = 0.0f;
    if (correlation_denominator > 0.0) {
        correlation = static_cast<float>(
            (pixels * overlap_pixels -
             static_cast<double>(scene_pixels) * sample.stroke_pixels) /
            correlation_denominator);
    }
    correlation = std::max(0.0f, std::min(1.0f,
                           (correlation + 1.0f) * 0.5f));

    return 0.30f * dice + 0.50f * chamfer + 0.20f * correlation;
}

RecognitionResult Recognizer::recognize(const cv::Mat& piece_roi) const {
    if (piece_roi.empty()) return RecognitionResult();

    cv::Mat scene;
    cv::threshold(piece_roi, scene, 128, 255, cv::THRESH_BINARY);
    const int scene_pixels = cv::countNonZero(scene);
    if (scene_pixels == 0) return RecognitionResult();

    cv::Mat inverse_scene, scene_distance;
    cv::bitwise_not(scene, inverse_scene);
    cv::distanceTransform(inverse_scene, scene_distance, cv::DIST_L2, 3);
    build_cache();

    RecognitionResult best;
    float second_best = 0.0f;

    for (const auto& character : cache_) {
        float best_char = 0.0f;
        for (const auto& variant : character.variants) {
            best_char = std::max(
                best_char,
                shape_score(scene, scene_distance, scene_pixels, variant));
        }

        if (best_char > best.confidence) {
            second_best = best.confidence;
            best.confidence = best_char;
            best.character_id = character.character_id;
            best.character_name = character.character_name;
        } else if (best_char > second_best) {
            second_best = best_char;
        }
    }

    best.margin = best.confidence - second_best;
    if (best.confidence < config_.confidence_threshold) {
        best.character_id = -1;
        best.character_name.clear();
    }
    return best;
}

} // namespace junqi
