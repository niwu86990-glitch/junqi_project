#include "junqi/preprocessor.h"
#include <opencv2/imgproc.hpp>

namespace junqi {

PreprocessedImage Preprocessor::process(const cv::Mat& input) const {
    PreprocessedImage result;

    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);

    cv::Mat denoised;
    cv::bilateralFilter(gray, denoised, 5, 50.0, 50.0);

    auto clahe = cv::createCLAHE(3.0, cv::Size(25, 25));
    clahe->apply(denoised, result.gray);

    cv::bilateralFilter(input, result.color, 5, 50.0, 50.0);

    return result;
}

} // namespace junqi
