#include "junqi/character_extractor.h"
#include "junqi/bg_utils.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace junqi {

    // 字符提取函数
cv::Mat CharacterExtractor::extract(const cv::Mat& piece_gray) const {
    cv::Mat working = piece_gray.clone();

    // 阶段一：截取棋子所在局部
    if (piece_gray.rows * piece_gray.cols > 50000) {
        cv::Mat p_bin;

        // 二值化+闭运算
        int piece_thresh = detect_thresh_type(working);
        cv::adaptiveThreshold(working, p_bin, 255,
                              cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              piece_thresh, 51, 8);
        cv::Mat pk = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7));
        cv::morphologyEx(p_bin, p_bin, cv::MORPH_CLOSE, pk);

        std::vector<std::vector<cv::Point>> p_cnts;
        cv::findContours(p_bin, p_cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (!p_cnts.empty()) {
            int bi = 0; double ba = 0;
            for (size_t i = 0; i < p_cnts.size(); i++) {
                double a = cv::contourArea(p_cnts[i]);
                if (a > ba) { ba = a; bi = i; }
            }
            cv::Rect p_bbox = cv::boundingRect(p_cnts[bi]);
            int px = static_cast<int>(p_bbox.width * 0.1);
            int py = static_cast<int>(p_bbox.height * 0.1);
            p_bbox.x = std::max(0, p_bbox.x - px);
            p_bbox.y = std::max(0, p_bbox.y - py);
            p_bbox.width  = std::min(working.cols - p_bbox.x, p_bbox.width  + 2 * px);
            p_bbox.height = std::min(working.rows - p_bbox.y, p_bbox.height + 2 * py);
            working = working(p_bbox).clone();
        }
    }

    // 阶段二：扣字
    cv::Mat binary;
    cv::adaptiveThreshold(working, binary, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY_INV, 21, 6);

    // Morph open to clean noise
    cv::Mat ck = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, ck);  //开运算

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return cv::Mat();

    double max_area = 0;
    for (const auto& c : contours)
        max_area = std::max(max_area, cv::contourArea(c));

    std::vector<std::vector<cv::Point>> kept;
    for (const auto& c : contours) {
        if (cv::contourArea(c) >= max_area * 0.05) kept.push_back(c);
    }
    if (kept.empty()) return cv::Mat();

    cv::Rect char_bbox = cv::boundingRect(kept[0]);
    for (size_t i = 1; i < kept.size(); i++)
        char_bbox |= cv::boundingRect(kept[i]);

    int pad_x = static_cast<int>(char_bbox.width * 0.15);
    int pad_y = static_cast<int>(char_bbox.height * 0.15);
    char_bbox.x = std::max(0, char_bbox.x - pad_x);
    char_bbox.y = std::max(0, char_bbox.y - pad_y);
    char_bbox.width  = std::min(working.cols - char_bbox.x, char_bbox.width  + 2 * pad_x);
    char_bbox.height = std::min(working.rows - char_bbox.y, char_bbox.height + 2 * pad_y);

    cv::Mat char_roi = working(char_bbox).clone();
    cv::Mat char_binary;
    cv::threshold(char_roi, char_binary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    // If Otsu inverted the strokes (background became white), flip back
    double wr = static_cast<double>(cv::countNonZero(char_binary))
              / (char_roi.rows * char_roi.cols);
    if (wr > 0.50)
        cv::bitwise_not(char_binary, char_binary);

    return char_binary;
}

// 使每张模版尺寸一致
cv::Mat CharacterExtractor::normalize(const cv::Mat& char_binary, cv::Size target_size) const {
    
    double char_aspect = static_cast<double>(char_binary.cols) / char_binary.rows;
    int fit_w = target_size.width;
    int fit_h = target_size.height;
    if (char_aspect > 1.0) {
        fit_h = static_cast<int>(target_size.height / char_aspect);
        if (fit_h < 8) fit_h = 8;
    } else {
        fit_w = static_cast<int>(target_size.width * char_aspect);
        if (fit_w < 8) fit_w = 8;
    }

    cv::Mat resized;
    cv::resize(char_binary, resized, cv::Size(fit_w, fit_h), 0, 0, cv::INTER_AREA);
    cv::threshold(resized, resized, 128, 255, cv::THRESH_BINARY);

    // Center in target_size with black padding
    int left = (target_size.width - fit_w) / 2;
    int top  = (target_size.height - fit_h) / 2;
    cv::Mat padded(target_size, CV_8UC1, cv::Scalar(0));
    resized.copyTo(padded(cv::Rect(left, top, fit_w, fit_h)));
    return padded;
}

} // namespace junqi
