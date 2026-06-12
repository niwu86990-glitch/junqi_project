#include "junqi/character_extractor.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace junqi {

cv::Mat CharacterExtractor::clean_and_crop(const cv::Mat& source) const {
    if (source.empty()) return cv::Mat();

    cv::Mat mask;
    if (source.type() == CV_8UC1) mask = source.clone();
    else source.convertTo(mask, CV_8UC1);
    cv::threshold(mask, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat open_kernel =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, open_kernel);

    const int image_area = mask.rows * mask.cols;
    const int border_x = std::max(2, mask.cols / 40);
    const int border_y = std::max(2, mask.rows / 40);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    double max_stroke_area = 0.0;
    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        const cv::Rect box = cv::boundingRect(contour);
        const double aspect =
            static_cast<double>(box.width) / std::max(1, box.height);
        if (area > image_area * 0.08 ||
            aspect > 12.0 || aspect < 1.0 / 12.0) {
            continue;
        }
        max_stroke_area = std::max(max_stroke_area, area);
    }

    cv::Mat cleaned(mask.size(), CV_8UC1, cv::Scalar(0));
    std::vector<std::vector<cv::Point>> kept;
    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        const cv::Rect box = cv::boundingRect(contour);
        const double min_stroke_area =
            std::max(std::max(3.0, image_area * 0.00015),
                     max_stroke_area * 0.01);
        if (area < min_stroke_area) continue;
        if (area > image_area * 0.20) continue;

        const double aspect =
            static_cast<double>(box.width) / std::max(1, box.height);
        const bool long_horizontal_texture =
            box.width > mask.cols * 0.45 &&
            box.height < mask.rows * 0.15;
        const bool long_vertical_texture =
            box.height > mask.rows * 0.45 &&
            box.width < mask.cols * 0.15;
        if (long_horizontal_texture || long_vertical_texture ||
            aspect > 12.0 || aspect < 1.0 / 12.0) {
            continue;
        }

        const bool border_component =
            box.x <= border_x || box.y <= border_y ||
            box.x + box.width >= mask.cols - border_x ||
            box.y + box.height >= mask.rows - border_y;
        if (border_component &&
            (box.width > mask.cols * 0.45 ||
             box.height > mask.rows * 0.45)) {
            continue;
        }
        kept.push_back(contour);
    }
    if (kept.empty()) return cv::Mat();

    cv::drawContours(cleaned, kept, -1, cv::Scalar(255), cv::FILLED);
    cv::bitwise_and(cleaned, mask, cleaned);

    // Join nearby strokes from the same Chinese character without merging the
    // piece border into the character.
    int join_size = std::max(1, std::min(mask.rows, mask.cols) / 120);
    cv::Mat join_kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(join_size * 2 + 1, join_size * 2 + 1));
    cv::morphologyEx(cleaned, cleaned, cv::MORPH_CLOSE, join_kernel);

    std::vector<cv::Point> points;
    cv::findNonZero(cleaned, points);
    if (points.empty()) return cv::Mat();

    cv::Rect box = cv::boundingRect(points);
    const double box_area = static_cast<double>(box.area());
    const double white_ratio =
        static_cast<double>(cv::countNonZero(cleaned(box))) /
        std::max(1.0, box_area);
    if (box.width < 8 || box.height < 8 ||
        white_ratio < 0.015 || white_ratio > 0.65) {
        return cv::Mat();
    }

    const int pad_x = std::max(2, box.width / 12);
    const int pad_y = std::max(2, box.height / 12);
    box = cv::Rect(box.x - pad_x, box.y - pad_y,
                   box.width + 2 * pad_x, box.height + 2 * pad_y);
    box &= cv::Rect(0, 0, cleaned.cols, cleaned.rows);
    return cleaned(box).clone();
}

std::vector<cv::Mat> CharacterExtractor::extract_candidates(
    const cv::Mat& piece_color, const cv::Mat& piece_gray) const {
    std::vector<cv::Mat> raw_masks;
    if (piece_gray.empty()) return raw_masks;

    cv::Mat gray;
    cv::Mat color_work = piece_color;
    if (!piece_color.empty() && piece_color.channels() == 3) {
        // Detection uses an illumination-normalized gray image, but character
        // extraction needs the original local contrast of black strokes.
        cv::cvtColor(piece_color, gray, cv::COLOR_BGR2GRAY);
    } else if (piece_gray.type() == CV_8UC1) {
        gray = piece_gray;
    } else {
        piece_gray.convertTo(gray, CV_8UC1);
    }

    // Text is printed inside the piece. Removing a narrow outer band prevents
    // the piece border and nearby table texture from competing with small
    // character strokes, while keeping generous placement tolerance.
    if (gray.cols >= 80 && gray.rows >= 60) {
        const int margin_x = gray.cols * 8 / 100;
        const int margin_y = gray.rows * 8 / 100;
        const cv::Rect inner(margin_x, margin_y,
                             gray.cols - 2 * margin_x,
                             gray.rows - 2 * margin_y);
        gray = gray(inner).clone();
        if (!color_work.empty()) color_work = color_work(inner).clone();
    }

    // Local illumination normalization: dark strokes remain dark even when one
    // side of the piece is in shadow and the other side has glare.
    int blur_size = std::max(15, (std::min(gray.rows, gray.cols) / 5) | 1);
    cv::Mat background;
    cv::GaussianBlur(gray, background, cv::Size(blur_size, blur_size), 0);

    cv::Mat gray_f, background_f, normalized_f, normalized;
    gray.convertTo(gray_f, CV_32F);
    background.convertTo(background_f, CV_32F);
    background_f += 1.0f;
    cv::divide(gray_f, background_f, normalized_f, 128.0);
    normalized_f.convertTo(normalized, CV_8U);

    cv::Mat adaptive;
    int block_size = std::max(21, (std::min(gray.rows, gray.cols) / 5) | 1);
    if (block_size > 61) block_size = 61;
    cv::adaptiveThreshold(normalized, adaptive, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY_INV, block_size, 7);
    raw_masks.push_back(adaptive);

    // Black-hat emphasizes strokes darker than their local neighborhood and is
    // insensitive to whether the surrounding table is black, gray or bright.
    int blackhat_size =
        std::max(9, (std::min(gray.rows, gray.cols) / 9) | 1);
    cv::Mat blackhat_kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(blackhat_size, blackhat_size));
    cv::Mat blackhat, blackhat_mask;
    cv::morphologyEx(gray, blackhat, cv::MORPH_BLACKHAT, blackhat_kernel);
    cv::threshold(blackhat, blackhat_mask, 0, 255,
                  cv::THRESH_BINARY | cv::THRESH_OTSU);
    raw_masks.push_back(blackhat_mask);

    if (!color_work.empty() && color_work.channels() == 3) {
        std::vector<cv::Mat> bgr;
        cv::split(color_work, bgr);

        // Red-excess survives many highlights better than grayscale intensity:
        // R - max(G, B). CLAHE restores weak red strokes after reflection.
        cv::Mat max_gb, red_excess;
        cv::max(bgr[0], bgr[1], max_gb);
        cv::subtract(bgr[2], max_gb, red_excess);
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(red_excess, red_excess);

        cv::Mat red_mask;
        cv::threshold(red_excess, red_mask, 0, 255,
                      cv::THRESH_BINARY | cv::THRESH_OTSU);

        cv::Mat hsv, saturation_mask;
        cv::cvtColor(color_work, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> hsv_channels;
        cv::split(hsv, hsv_channels);
        cv::threshold(hsv_channels[1], saturation_mask, 25, 255,
                      cv::THRESH_BINARY);
        cv::bitwise_and(red_mask, saturation_mask, red_mask);
        raw_masks.push_back(red_mask);

        cv::Mat combined;
        cv::bitwise_or(adaptive, red_mask, combined);
        raw_masks.push_back(combined);
    }

    std::vector<cv::Mat> candidates;
    for (const auto& mask : raw_masks) {
        cv::Mat candidate = clean_and_crop(mask);
        if (candidate.empty()) continue;

        bool duplicate = false;
        cv::Mat normalized_candidate = normalize(candidate, cv::Size(64, 64));
        for (const auto& existing : candidates) {
            cv::Mat normalized_existing = normalize(existing, cv::Size(64, 64));
            cv::Mat difference;
            cv::bitwise_xor(normalized_candidate, normalized_existing,
                            difference);
            const double ratio =
                static_cast<double>(cv::countNonZero(difference)) /
                difference.total();
            if (ratio < 0.02) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) candidates.push_back(candidate);
    }
    return candidates;
}

cv::Mat CharacterExtractor::extract(const cv::Mat& piece_gray) const {
    auto candidates = extract_candidates(cv::Mat(), piece_gray);
    return candidates.empty() ? cv::Mat() : candidates.front();
}

cv::Mat CharacterExtractor::normalize(const cv::Mat& char_binary,
                                       cv::Size target_size) const {
    if (char_binary.empty()) return cv::Mat();

    cv::Mat binary;
    cv::threshold(char_binary, binary, 0, 255,
                  cv::THRESH_BINARY | cv::THRESH_OTSU);

    std::vector<cv::Point> points;
    cv::findNonZero(binary, points);
    if (points.empty()) return cv::Mat();
    binary = binary(cv::boundingRect(points)).clone();

    const int margin = std::max(3, target_size.width / 12);
    const int available_w = target_size.width - 2 * margin;
    const int available_h = target_size.height - 2 * margin;
    const double scale = std::min(
        static_cast<double>(available_w) / binary.cols,
        static_cast<double>(available_h) / binary.rows);
    const int fit_w = std::max(1, static_cast<int>(binary.cols * scale));
    const int fit_h = std::max(1, static_cast<int>(binary.rows * scale));

    cv::Mat resized;
    cv::resize(binary, resized, cv::Size(fit_w, fit_h), 0, 0,
               scale < 1.0 ? cv::INTER_AREA : cv::INTER_NEAREST);
    cv::threshold(resized, resized, 128, 255, cv::THRESH_BINARY);

    cv::Mat padded(target_size, CV_8UC1, cv::Scalar(0));
    const int left = (target_size.width - fit_w) / 2;
    const int top = (target_size.height - fit_h) / 2;
    resized.copyTo(padded(cv::Rect(left, top, fit_w, fit_h)));
    return padded;
}

} // namespace junqi
