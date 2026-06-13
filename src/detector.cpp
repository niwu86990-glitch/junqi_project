#include "junqi/detector.h"
#include "junqi/bg_utils.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace junqi {

std::vector<DetectedPiece> Detector::detect(const cv::Mat& gray, const cv::Mat& color) const {
    int thresh_type = detect_thresh_type(gray);

    // Primary: Otsu binarization
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, thresh_type | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);

    // Check if Otsu found plausible piece regions before falling back
    double min_area = gray.rows * gray.cols * 0.01;
    double max_area = gray.rows * gray.cols * 0.60;
    {
        std::vector<std::vector<cv::Point>> test_cnts;
        cv::findContours(binary, test_cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        bool has_valid = false;
        for (const auto& c : test_cnts) {
            double a = cv::contourArea(c);
            cv::Rect r = cv::boundingRect(c);
            double asp = static_cast<double>(r.width) / r.height;
            if (a >= min_area && a <= max_area && asp >= 0.9 && asp <= 2.2) {
                has_valid = true;
                break;
            }
        }
        if (!has_valid) {
            // Fallback: adaptive threshold
            cv::adaptiveThreshold(gray, binary, 255,
                                  cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                                  thresh_type, 51, 10);
            cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
        }
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double img_width = gray.cols;

    std::vector<DetectedPiece> pieces;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area < min_area || area > max_area) continue;

        cv::Rect bbox = cv::boundingRect(contour);
        double aspect = static_cast<double>(bbox.width) / bbox.height;

        // Skip contours that nearly span the full image width (merged pieces)
        if (bbox.width > img_width * 0.75) continue;

        // For scene photos, relax aspect constraints
        if (aspect < 0.9 || aspect > 2.2) continue;

        DetectedPiece piece;
        piece.bounding_box = bbox;
        piece.rotated_box = cv::minAreaRect(contour);
        piece.roi_gray = gray(bbox).clone();
        piece.roi_color = color(bbox).clone();
        pieces.push_back(piece);
    }

    // If 0 or 1 piece found, split at image center
    if (pieces.size() <= 1) {
        pieces.clear();
        bool portrait = gray.rows > gray.cols;

        DetectedPiece p1, p2;
        p1.is_left = true;
        p2.is_left = false;

        if (portrait) {
            // Top-bottom layout: split horizontally
            int mid_y = gray.rows / 2;
            p1.bounding_box = cv::Rect(0, 0, gray.cols, mid_y);
            p2.bounding_box = cv::Rect(0, mid_y, gray.cols, gray.rows - mid_y);
        } else {
            // Left-right layout: split vertically
            int mid_x = gray.cols / 2;
            p1.bounding_box = cv::Rect(0, 0, mid_x, gray.rows);
            p2.bounding_box = cv::Rect(mid_x, 0, gray.cols - mid_x, gray.rows);
        }

        p1.roi_gray = gray(p1.bounding_box).clone();
        p1.roi_color = color(p1.bounding_box).clone();
        p2.roi_gray = gray(p2.bounding_box).clone();
        p2.roi_color = color(p2.bounding_box).clone();
        pieces.push_back(p1);
        pieces.push_back(p2);

        return pieces;
    }

    std::sort(pieces.begin(), pieces.end(),
              [](const DetectedPiece& a, const DetectedPiece& b) {
                  return a.bounding_box.x < b.bounding_box.x;
              });

    if (!pieces.empty()) pieces[0].is_left = true;
    if (pieces.size() >= 2) pieces[1].is_left = false;

    return pieces;
}

DetectedPiece Detector::detect_single(const cv::Mat& gray, const cv::Mat& color) const {
    const double image_area = static_cast<double>(gray.rows) * gray.cols;
    const double min_area = image_area * 0.005;
    const double max_area = image_area * 0.80;
    cv::Rect best_box;
    double best_score = -1e9;
    const cv::Point2d image_center(gray.cols / 2.0, gray.rows / 2.0);

    // Real board captures usually contain one light, low-saturation piece on
    // a darker table. Detect that rectangle first; it is both more reliable
    // and much cheaper than scoring every threshold polarity over the frame.
    if (!color.empty() && color.channels() == 3) {
        cv::Mat hsv;
        cv::cvtColor(color, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> channels;
        cv::split(hsv, channels);

        cv::Mat bright, low_saturation, piece_mask;
        const double otsu_level =
            cv::threshold(channels[2], bright, 0, 255,
                          cv::THRESH_BINARY | cv::THRESH_OTSU);
        const double minimum_brightness = std::max(135.0, otsu_level);
        cv::threshold(channels[2], bright, minimum_brightness, 255,
                      cv::THRESH_BINARY);
        cv::threshold(channels[1], low_saturation, 125, 255,
                      cv::THRESH_BINARY_INV);
        cv::bitwise_and(bright, low_saturation, piece_mask);

        const cv::Mat close_kernel = cv::getStructuringElement(
            cv::MORPH_RECT, cv::Size(11, 11));
        const cv::Mat open_kernel = cv::getStructuringElement(
            cv::MORPH_RECT, cv::Size(3, 3));
        cv::morphologyEx(piece_mask, piece_mask, cv::MORPH_CLOSE,
                         close_kernel);
        cv::morphologyEx(piece_mask, piece_mask, cv::MORPH_OPEN,
                         open_kernel);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(piece_mask, contours, cv::RETR_EXTERNAL,
                         cv::CHAIN_APPROX_SIMPLE);

        cv::Rect bright_box;
        double bright_score = -1e9;
        for (const auto& contour : contours) {
            const double area = cv::contourArea(contour);
            if (area < image_area * 0.004 ||
                area > image_area * 0.25) {
                continue;
            }

            const cv::Rect box = cv::boundingRect(contour);
            if (box.width < 30 || box.height < 25) continue;
            const double aspect =
                static_cast<double>(box.width) / box.height;
            if (aspect < 0.70 || aspect > 2.60) continue;

            const double rectangularity =
                area / std::max(1, box.area());
            if (rectangularity < 0.55) continue;

            const cv::Point2d center(box.x + box.width / 2.0,
                                     box.y + box.height / 2.0);
            const double diagonal = std::sqrt(
                static_cast<double>(gray.cols * gray.cols +
                                    gray.rows * gray.rows));
            const double center_score =
                1.0 - std::min(1.0, cv::norm(center - image_center) /
                                      diagonal);
            const double aspect_score =
                std::exp(-std::abs(std::log(aspect / 1.45)));
            const double score =
                rectangularity * 4.0 + aspect_score * 2.0 +
                center_score * 0.5 + std::min(area / image_area, 0.08) * 4.0;
            if (score > bright_score) {
                bright_score = score;
                bright_box = box;
            }
        }

        if (bright_box.area() > 0) {
            const int px = std::max(3, bright_box.width / 18);
            const int py = std::max(3, bright_box.height / 18);
            bright_box = cv::Rect(bright_box.x - px, bright_box.y - py,
                                  bright_box.width + 2 * px,
                                  bright_box.height + 2 * py);
            bright_box &= cv::Rect(0, 0, gray.cols, gray.rows);

            DetectedPiece piece;
            piece.bounding_box = bright_box;
            piece.rotated_box = cv::RotatedRect();
            piece.roi_gray = gray(bright_box).clone();
            piece.roi_color = color(bright_box).clone();
            piece.is_left = true;
            return piece;
        }
    }

    std::vector<cv::Mat> masks;
    cv::Mat raw_gray;
    if (!color.empty() && color.channels() == 3) {
        cv::cvtColor(color, raw_gray, cv::COLOR_BGR2GRAY);
    } else {
        raw_gray = gray;
    }

    cv::Mat raw_otsu, raw_edges;
    cv::threshold(raw_gray, raw_otsu, 0, 255,
                  cv::THRESH_BINARY | cv::THRESH_OTSU);
    masks.push_back(raw_otsu);
    cv::bitwise_not(raw_otsu, raw_otsu);
    masks.push_back(raw_otsu);
    cv::Canny(raw_gray, raw_edges, 35, 110);
    cv::Mat raw_edge_kernel =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(13, 13));
    cv::morphologyEx(raw_edges, raw_edges, cv::MORPH_CLOSE, raw_edge_kernel);
    masks.push_back(raw_edges);

    cv::Mat otsu, adaptive, edges;
    cv::threshold(gray, otsu, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    masks.push_back(otsu);
    cv::bitwise_not(otsu, otsu);
    masks.push_back(otsu);

    cv::adaptiveThreshold(gray, adaptive, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, 51, 7);
    masks.push_back(adaptive);
    cv::bitwise_not(adaptive, adaptive);
    masks.push_back(adaptive);

    cv::Canny(gray, edges, 45, 135);
    cv::Mat edge_kernel =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9));
    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, edge_kernel);
    masks.push_back(edges);

    for (cv::Mat mask : masks) {
        cv::Mat close_kernel =
            cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
        cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, close_kernel);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_LIST,
                         cv::CHAIN_APPROX_SIMPLE);
        for (const auto& contour : contours) {
            const double area = cv::contourArea(contour);
            if (area < min_area || area > max_area) continue;

            const cv::Rect box = cv::boundingRect(contour);
            if (box.width < 30 || box.height < 30) continue;
            if (box.width > gray.cols * 0.90 ||
                box.height > gray.rows * 0.90) continue;
            const double aspect = static_cast<double>(box.width) / box.height;
            if (aspect < 0.65 || aspect > 2.8) continue;

            const double rectangularity = area / std::max(1, box.area());
            if (rectangularity < 0.35) continue;

            const cv::Point2d center(box.x + box.width / 2.0,
                                     box.y + box.height / 2.0);
            const double distance = cv::norm(center - image_center);
            const double diagonal = std::sqrt(
                static_cast<double>(gray.cols * gray.cols +
                                    gray.rows * gray.rows));
            const double center_score = 1.0 - std::min(1.0, distance / diagonal);
            const double area_ratio = area / image_area;
            const bool touches_border =
                box.x <= 1 || box.y <= 1 ||
                box.x + box.width >= gray.cols - 1 ||
                box.y + box.height >= gray.rows - 1;
            if (touches_border && area_ratio > 0.15) continue;

            // Junqi pieces are moderately wide rectangles. Keep this as a soft
            // preference so off-center placement and perspective still work.
            const double aspect_score =
                std::exp(-std::abs(std::log(aspect / 1.55)));

            double score = rectangularity * 3.0 +
                           center_score * 1.2 +
                           std::min(0.35, area_ratio) * 4.0 +
                           aspect_score * 1.4;
            if (touches_border) score -= 1.5;
            if (score > best_score) {
                best_score = score;
                best_box = box;
            }
        }
    }

    // Keep a broad central fallback, but discard the frame border where
    // unrelated background edges and lighting gradients are strongest.
    if (best_box.area() <= 0) {
        const int mx = gray.cols / 10;
        const int my = gray.rows / 10;
        best_box = cv::Rect(mx, my, gray.cols - 2 * mx, gray.rows - 2 * my);
    } else {
        const int px = std::max(4, best_box.width / 20);
        const int py = std::max(4, best_box.height / 20);
        best_box = cv::Rect(best_box.x - px, best_box.y - py,
                            best_box.width + 2 * px,
                            best_box.height + 2 * py);
        best_box &= cv::Rect(0, 0, gray.cols, gray.rows);
    }

    DetectedPiece piece;
    piece.bounding_box = best_box;
    piece.rotated_box = cv::RotatedRect();
    piece.roi_gray = gray(best_box).clone();
    piece.roi_color = color(best_box).clone();
    piece.is_left = true;
    return piece;
}

} // namespace junqi
