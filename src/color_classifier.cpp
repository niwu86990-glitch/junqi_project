#include "junqi/color_classifier.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <vector>

namespace junqi {
namespace {

double median_channel(const cv::Mat& channel) {
    std::vector<unsigned char> values;
    values.assign(channel.datastart, channel.dataend);
    const size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    return values[middle];
}

} // namespace

cv::Mat ColorClassifier::buildHighlightMask(const cv::Mat& hsv) const {
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);

    cv::Scalar mean_v, std_v;
    cv::meanStdDev(channels[2], mean_v, std_v);
    double hi_thresh = mean_v[0] + 2.0 * std_v[0];
    hi_thresh = std::min(255.0, std::max(205.0, hi_thresh));

    cv::Mat high_value, low_saturation, highlight;
    cv::inRange(channels[2], hi_thresh, 255, high_value);
    cv::inRange(channels[1], 0, 35, low_saturation);
    cv::bitwise_and(high_value, low_saturation, highlight);
    return highlight;
}

PieceColor ColorClassifier::classify(const cv::Mat& piece_color_roi) const {
    if (piece_color_roi.empty()) return PieceColor::UNKNOWN;

    cv::Mat color_roi = piece_color_roi;
    if (piece_color_roi.cols >= 80 && piece_color_roi.rows >= 60) {
        const int mx = piece_color_roi.cols / 10;
        const int my = piece_color_roi.rows / 10;
        color_roi = piece_color_roi(
            cv::Rect(mx, my, piece_color_roi.cols - 2 * mx,
                     piece_color_roi.rows - 2 * my));
    }

    cv::Mat gray, hsv, lab, ycrcb;
    cv::cvtColor(color_roi, gray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(color_roi, hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(color_roi, lab, cv::COLOR_BGR2Lab);
    cv::cvtColor(color_roi, ycrcb, cv::COLOR_BGR2YCrCb);

    std::vector<cv::Mat> bgr_channels, hsv_channels;
    std::vector<cv::Mat> lab_channels, ycrcb_channels;
    cv::split(color_roi, bgr_channels);
    cv::split(hsv, hsv_channels);
    cv::split(lab, lab_channels);
    cv::split(ycrcb, ycrcb_channels);

    // Estimate the piece's current neutral color. Relative chroma offsets are
    // more stable than fixed thresholds under exposure and white-balance drift.
    const double median_a = median_channel(lab_channels[1]);
    const double median_cr = median_channel(ycrcb_channels[1]);

    cv::Mat max_gb, red_excess;
    cv::max(bgr_channels[0], bgr_channels[1], max_gb);
    cv::subtract(bgr_channels[2], max_gb, red_excess);

    cv::Mat red_rgb, red_lab, red_cr;
    cv::threshold(red_excess, red_rgb, 8, 255, cv::THRESH_BINARY);
    cv::threshold(lab_channels[1], red_lab,
                  std::min(255.0, median_a + 5.0), 255,
                  cv::THRESH_BINARY);
    cv::threshold(ycrcb_channels[1], red_cr,
                  std::min(255.0, median_cr + 6.0), 255,
                  cv::THRESH_BINARY);

    // A red pixel must satisfy at least two independent color descriptions.
    cv::Mat rgb_lab, rgb_cr, lab_cr, red_seed;
    cv::bitwise_and(red_rgb, red_lab, rgb_lab);
    cv::bitwise_and(red_rgb, red_cr, rgb_cr);
    cv::bitwise_and(red_lab, red_cr, lab_cr);
    cv::bitwise_or(rgb_lab, rgb_cr, red_seed);
    cv::bitwise_or(red_seed, lab_cr, red_seed);

    cv::Mat saturation_ok, value_ok;
    cv::threshold(hsv_channels[1], saturation_ok, 10, 255,
                  cv::THRESH_BINARY);
    cv::threshold(hsv_channels[2], value_ok, 25, 255,
                  cv::THRESH_BINARY);
    cv::bitwise_and(red_seed, saturation_ok, red_seed);
    cv::bitwise_and(red_seed, value_ok, red_seed);

    cv::Mat highlight = buildHighlightMask(hsv);
    cv::Mat red_neighborhood;
    cv::Mat recovery_kernel =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::dilate(red_seed, red_neighborhood, recovery_kernel);

    // A white highlight is accepted as part of a red stroke only when red
    // evidence surrounds it. Pure glare elsewhere on the piece is ignored.
    cv::Mat recovered_highlight;
    cv::bitwise_and(highlight, red_neighborhood, recovered_highlight);
    cv::Mat red_mask;
    cv::bitwise_or(red_seed, recovered_highlight, red_mask);

    cv::Mat clean_kernel =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(red_mask, red_mask, cv::MORPH_CLOSE, clean_kernel);

    // Black strokes are locally dark and nearly neutral. Requiring low red
    // chroma keeps dim red pixels from being counted as black.
    cv::Mat dark_mask, low_red_excess, low_saturation, black_mask;
    cv::threshold(gray, dark_mask, 95, 255, cv::THRESH_BINARY_INV);
    cv::threshold(red_excess, low_red_excess, 7, 255,
                  cv::THRESH_BINARY_INV);
    cv::threshold(hsv_channels[1], low_saturation, 65, 255,
                  cv::THRESH_BINARY_INV);
    cv::bitwise_and(dark_mask, low_red_excess, black_mask);
    cv::bitwise_and(black_mask, low_saturation, black_mask);

    const int roi_area = color_roi.rows * color_roi.cols;
    const int red_pixels = cv::countNonZero(red_mask);
    const int red_core_pixels = cv::countNonZero(red_seed);
    const int black_pixels = cv::countNonZero(black_mask);

    // Connected components reject isolated reddish noise from the table or
    // compression artifacts while preserving separated Chinese strokes.
    cv::Mat labels, stats, centroids;
    const int component_count =
        cv::connectedComponentsWithStats(red_seed, labels, stats, centroids,
                                         8, CV_32S);
    int meaningful_red_pixels = 0;
    const int min_component = std::max(3, roi_area / 4000);
    for (int i = 1; i < component_count; ++i) {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= min_component) meaningful_red_pixels += area;
    }

    const int min_red = std::max(12, roi_area / 700);
    const int min_black = std::max(18, roi_area / 500);

    if (meaningful_red_pixels >= min_red &&
        red_core_pixels > black_pixels * 0.12) {
        return PieceColor::RED;
    }
    if (black_pixels >= min_black &&
        red_pixels < black_pixels * 0.18) {
        return PieceColor::BLACK;
    }
    if (meaningful_red_pixels >= min_red / 2 &&
        red_pixels > black_pixels * 0.30) {
        return PieceColor::RED;
    }
    return PieceColor::UNKNOWN;
}

} // namespace junqi
