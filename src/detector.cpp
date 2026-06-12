#include "junqi/detector.h"
#include "junqi/bg_utils.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
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

} // namespace junqi
