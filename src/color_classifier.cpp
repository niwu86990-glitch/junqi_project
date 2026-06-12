#include "junqi/color_classifier.h"
#include <opencv2/imgproc.hpp>

namespace junqi {

cv::Mat ColorClassifier::buildHighlightMask(const cv::Mat& hsv) const {
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);

    // Dynamic highlight threshold: mean_V + 2 * stddev_V
    cv::Scalar mean_v, std_v;
    cv::meanStdDev(channels[2], mean_v, std_v);
    double hi_thresh = mean_v[0] + 2.0 * std_v[0];
    hi_thresh = std::min(255.0, hi_thresh);
    hi_thresh = std::max(200.0, hi_thresh);

    cv::Mat high_val, low_sat;
    cv::inRange(channels[2], hi_thresh, 255, high_val);
    cv::inRange(channels[1], 0, 50, low_sat);

    cv::Mat highlight;
    cv::bitwise_and(high_val, low_sat, highlight);

    // Dark pixels (black ink or shadow)
    cv::Mat dark;
    cv::inRange(channels[2], 0, 60, dark);

    // Near-white background
    cv::Mat bg;
    cv::inRange(channels[2], 200, 255, bg);

    cv::Mat invalid;
    cv::bitwise_or(highlight, dark, invalid);
    cv::bitwise_or(invalid, bg, invalid);

    cv::Mat valid;
    cv::bitwise_not(invalid, valid);
    return valid;
}

PieceColor ColorClassifier::classify(const cv::Mat& piece_color_roi) const {
    // §2.1: Gradient-based stroke mask (excludes background texture/shadows)
    cv::Mat gray;
    cv::cvtColor(piece_color_roi, gray, cv::COLOR_BGR2GRAY);

    cv::Mat gx, gy;
    cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
    cv::Mat grad = cv::abs(gx) + cv::abs(gy);
    grad.convertTo(grad, CV_8U);

    cv::Mat stroke_mask;
    cv::threshold(grad, stroke_mask, 20, 255, cv::THRESH_BINARY);

    // Fallback to Otsu if gradient mask is too sparse (< 3% of total pixels)
    double grad_ratio = static_cast<double>(cv::countNonZero(stroke_mask))
                      / (stroke_mask.rows * stroke_mask.cols);
    if (grad_ratio < 0.05) {
        cv::threshold(gray, stroke_mask, 0, 255,
                      cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    }

    // Convert to multiple color spaces
    cv::Mat hsv, lab, ycrcb;
    cv::cvtColor(piece_color_roi, hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(piece_color_roi, lab, cv::COLOR_BGR2Lab);
    cv::cvtColor(piece_color_roi, ycrcb, cv::COLOR_BGR2YCrCb);

    // Valid-stroke masks
    cv::Mat red_valid_mask = buildHighlightMask(hsv);
    cv::Mat red_valid_stroke;
    cv::bitwise_and(stroke_mask, red_valid_mask, red_valid_stroke);

    cv::Mat black_valid_mask;
    {
        std::vector<cv::Mat> hsv_ch;
        cv::split(hsv, hsv_ch);

        cv::Scalar mean_v2, std_v2;
        cv::meanStdDev(hsv_ch[2], mean_v2, std_v2);
        double dyn_hi = std::min(255.0, std::max(200.0, mean_v2[0] + 2.0 * std_v2[0]));

        cv::Mat high_val, low_sat;
        cv::inRange(hsv_ch[2], dyn_hi, 255, high_val);
        cv::inRange(hsv_ch[1], 0, 50, low_sat);
        cv::Mat highlight;
        cv::bitwise_and(high_val, low_sat, highlight);

        cv::Mat bg;
        cv::inRange(hsv_ch[2], 200, 255, bg);

        cv::Mat black_invalid;
        cv::bitwise_or(highlight, bg, black_invalid);
        cv::bitwise_not(black_invalid, black_valid_mask);
    }
    cv::Mat black_valid_stroke;
    cv::bitwise_and(stroke_mask, black_valid_mask, black_valid_stroke);

    // --- HSV red vote (§2.3: relaxed thresholds) ---
    std::vector<cv::Mat> hsv_ch;
    cv::split(hsv, hsv_ch);
    cv::Mat red_hue1, red_hue2, red_hue;
    cv::inRange(hsv_ch[0], 0, 20, red_hue1);
    cv::inRange(hsv_ch[0], 165, 180, red_hue2);
    cv::bitwise_or(red_hue1, red_hue2, red_hue);
    cv::Mat red_sat, red_val;
    cv::inRange(hsv_ch[1], 30, 255, red_sat);
    cv::inRange(hsv_ch[2], 30, 255, red_val);
    cv::Mat hsv_red;
    cv::bitwise_and(red_hue, red_sat, hsv_red);
    cv::bitwise_and(hsv_red, red_val, hsv_red);
    cv::bitwise_and(hsv_red, red_valid_stroke, hsv_red);
    int red_votes = cv::countNonZero(hsv_red);

    // --- Lab a* red vote ---
    std::vector<cv::Mat> lab_ch;
    cv::split(lab, lab_ch);
    cv::Mat lab_red;
    cv::inRange(lab_ch[1], 140, 255, lab_red);
    cv::bitwise_and(lab_red, red_valid_stroke, lab_red);
    red_votes += cv::countNonZero(lab_red);

    // --- YCrCb Cr red vote ---
    std::vector<cv::Mat> ycrcb_ch;
    cv::split(ycrcb, ycrcb_ch);
    cv::Mat ycrcb_red;
    cv::inRange(ycrcb_ch[1], 145, 255, ycrcb_red);
    cv::bitwise_and(ycrcb_red, red_valid_stroke, ycrcb_red);
    red_votes += cv::countNonZero(ycrcb_red);

    // --- Black votes (§2.3: relaxed thresholds) ---
    cv::Mat black_v;
    cv::inRange(hsv_ch[2], 0, 90, black_v);
    cv::bitwise_and(black_v, black_valid_stroke, black_v);
    int black_votes = cv::countNonZero(black_v);

    cv::Mat black_l;
    cv::inRange(lab_ch[0], 0, 80, black_l);
    cv::bitwise_and(black_l, black_valid_stroke, black_l);
    black_votes += cv::countNonZero(black_l);

    cv::Mat black_y;
    cv::inRange(ycrcb_ch[0], 0, 90, black_y);
    cv::bitwise_and(black_y, black_valid_stroke, black_y);
    black_votes += cv::countNonZero(black_y);

    if (red_votes > black_votes * 1.5) return PieceColor::RED;
    if (black_votes > red_votes * 1.5) return PieceColor::BLACK;
    return PieceColor::UNKNOWN;
}

} // namespace junqi
