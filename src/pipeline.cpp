#include "junqi/pipeline.h"
#include "junqi/bg_utils.h"
#include <opencv2/imgproc.hpp>
#include <chrono>
#include <algorithm>

namespace junqi {

Pipeline::Pipeline(const Config& config)
    : config_(config)
    , recognizer_(library_)
{
    library_.load(config.template_dir);
}

bool Pipeline::is_ready() const {
    return library_.size() > 0;
}

PieceResult Pipeline::process_single_piece(const cv::Mat& piece_color,
                                            const cv::Mat& piece_gray,
                                            const cv::Rect& bbox) {
    PieceResult result;
    result.bounding_box = bbox;

    cv::Mat d_color = piece_color;
    cv::Mat d_gray = piece_gray;

    // Deskew
    cv::Mat binary;
    int pipe_thresh = detect_thresh_type(piece_gray);
    cv::adaptiveThreshold(piece_gray, binary, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          pipe_thresh, 51, 8);
    cv::Mat k = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, k);

    std::vector<std::vector<cv::Point>> cnts;
    cv::findContours(binary, cnts, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (!cnts.empty()) {
        int bi = 0; double ba = 0;
        for (size_t i = 0; i < cnts.size(); i++) {
            double a = cv::contourArea(cnts[i]);
            if (a > ba) { ba = a; bi = i; }
        }
        cv::RotatedRect rr = cv::minAreaRect(cnts[bi]);
        double ang = rr.angle;
        cv::Size sz = rr.size;
        if (sz.width < sz.height) { ang += 90.0; std::swap(sz.width, sz.height); }

        cv::Mat rot = cv::getRotationMatrix2D(rr.center, ang, 1.0);
        cv::warpAffine(piece_color, d_color, rot, piece_color.size(),
                       cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
        cv::cvtColor(d_color, d_gray, cv::COLOR_BGR2GRAY);

        cv::Rect crop(
            std::max(0, static_cast<int>(rr.center.x - sz.width / 2)),
            std::max(0, static_cast<int>(rr.center.y - sz.height / 2)),
            std::min(d_color.cols, static_cast<int>(sz.width)),
            std::min(d_color.rows, static_cast<int>(sz.height)));
        crop &= cv::Rect(0, 0, d_color.cols, d_color.rows);
        if (crop.area() > 0) {
            d_color = d_color(crop).clone();
            d_gray = d_gray(crop).clone();
        }
    }

    // Color classification
    result.color = color_classifier_.classify(d_color);

    // Character extraction: binarize strokes and normalize to 64x64
    cv::Mat char_bin = character_extractor_.extract(d_gray);
    if (!char_bin.empty()) {
        cv::Mat norm = character_extractor_.normalize(char_bin, cv::Size(64, 64));
        auto rec = recognizer_.recognize(norm);
        result.character_id = rec.character_id;
        result.character = rec.character_name;
        result.confidence = rec.confidence;
    }

    return result;
}

RecognitionOutput Pipeline::process(const cv::Mat& input_image) {
    RecognitionOutput output;
    auto t_start = std::chrono::high_resolution_clock::now();

    if (input_image.empty()) {
        output.success = false;
        output.error_message = "Empty input image";
        return output;
    }

    if (!is_ready()) {
        output.success = false;
        output.error_message = "Template library not loaded";
        return output;
    }

    auto preprocessed = preprocessor_.process(input_image);
    auto pieces = detector_.detect(preprocessed.gray, preprocessed.color);

    if (pieces.size() < 2) {
        output.success = false;
        output.error_message = "Expected 2 pieces, detected " +
                               std::to_string(pieces.size());
        // 仍然尝试处理已检测到的棋子（0 或 1 枚），返回部分结果
    }

    for (auto& piece : pieces) {
        PieceResult result = process_single_piece(
            piece.roi_color, piece.roi_gray, piece.bounding_box);
        if (piece.is_left) output.left_piece = result;
        else output.right_piece = result;
    }

    output.success = (pieces.size() >= 2);
    auto t_end = std::chrono::high_resolution_clock::now();
    output.elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    return output;
}

} // namespace junqi
