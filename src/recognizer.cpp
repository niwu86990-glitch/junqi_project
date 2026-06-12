#include "junqi/recognizer.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace junqi {

Recognizer::Recognizer(const TemplateLibrary& lib, const RecognizerConfig& cfg)
    : library_(lib), config_(cfg) {}

cv::Mat Recognizer::rotate_image(const cv::Mat& src, double angle) const {
    cv::Point2f center(src.cols / 2.0f, src.rows / 2.0f);
    cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::Mat dst;
    cv::warpAffine(src, dst, rot, src.size(), cv::INTER_CUBIC,
                   cv::BORDER_CONSTANT, cv::Scalar(0));
    return dst;
}

RecognitionResult Recognizer::recognize(const cv::Mat& piece_roi) const {
    if (piece_roi.empty()) return {};

    // CLAHE to normalize contrast
    cv::Mat scene;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
    clahe->apply(piece_roi, scene);

    RecognitionResult best;
    best.confidence = 0.0f;

    for (const auto& tmpl : library_.templates()) {
        float best_char = 0.0f;

        for (const auto& sample : tmpl.samples) {
            for (double angle = config_.rot_start; angle <= config_.rot_end;
                 angle += config_.rot_step) {
                cv::Mat rotated = rotate_image(sample, angle);
                cv::threshold(rotated, rotated, 128, 255, cv::THRESH_BINARY);

                for (double scale = config_.scale_start; scale <= config_.scale_end;
                     scale += config_.scale_step) {
                    cv::Mat scaled;
                    cv::resize(rotated, scaled, cv::Size(), scale, scale, cv::INTER_AREA);
                    cv::threshold(scaled, scaled, 128, 255, cv::THRESH_BINARY);

                    // Ensure scene >= template (pad if needed)
                    cv::Mat img, tpl;
                    if (scene.rows >= scaled.rows && scene.cols >= scaled.cols) {
                        img = scene; tpl = scaled;
                    } else {
                        int pr = std::max(0, scaled.rows - scene.rows);
                        int pc = std::max(0, scaled.cols - scene.cols);
                        cv::copyMakeBorder(scene, img, 0, pr, 0, pc,
                                          cv::BORDER_CONSTANT, cv::Scalar(0));
                        tpl = scaled;
                    }
                    cv::Mat result;
                    cv::matchTemplate(img, tpl, result, cv::TM_CCOEFF_NORMED);
                    double minVal, maxVal;
                    cv::minMaxLoc(result, &minVal, &maxVal);
                    if (maxVal > best_char) best_char = static_cast<float>(maxVal);
                }
            }
        }

        if (best_char > best.confidence) {
            best.confidence = best_char;
            best.character_id = tmpl.character_id;
            best.character_name = tmpl.chinese_name;
        }
    }

    if (best.confidence < config_.confidence_threshold) {
        best.character_id = -1;
        best.character_name.clear();
    }

    return best;
}

} // namespace junqi
