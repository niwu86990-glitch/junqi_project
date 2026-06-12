#include "junqi/preprocessor.h"
#include <opencv2/imgproc.hpp>

namespace junqi {

PreprocessedImage Preprocessor::process(const cv::Mat& input) const {
    PreprocessedImage result;

    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);

    // Estimate slow-changing illumination and divide it out. This keeps piece
    // edges visible under shadows and hot spots without assuming a dark border.
    int blur_size = std::max(31, (std::min(gray.rows, gray.cols) / 6) | 1);
    cv::Mat illumination;
    cv::GaussianBlur(gray, illumination, cv::Size(blur_size, blur_size), 0);

    cv::Mat gray_f, illumination_f, normalized_f, normalized;
    gray.convertTo(gray_f, CV_32F);
    illumination.convertTo(illumination_f, CV_32F);
    illumination_f += 1.0f;
    cv::divide(gray_f, illumination_f, normalized_f, 128.0);
    normalized_f.convertTo(normalized, CV_8U);

    cv::Mat denoised;
    cv::bilateralFilter(normalized, denoised, 5, 35.0, 35.0);

    auto clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(denoised, result.gray);

    // Preserve chroma for red/black classification; heavy color smoothing
    // blends thin red strokes into reflective piece surfaces.
    cv::bilateralFilter(input, result.color, 5, 30.0, 30.0);

    return result;
}

} // namespace junqi
