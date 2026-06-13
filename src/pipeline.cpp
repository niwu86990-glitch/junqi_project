#include "junqi/pipeline.h"
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

    // Multiple extraction strategies avoid tying recognition to one lighting
    // condition or one assumed background polarity.
    auto candidates =
        character_extractor_.extract_candidates(piece_color, piece_gray);
    RecognitionResult best;
    for (const auto& char_bin : candidates) {
        cv::Mat norm =
            character_extractor_.normalize(char_bin, cv::Size(64, 64));
        auto rec = recognizer_.recognize(norm);
        if (rec.confidence > best.confidence) best = rec;
    }
    if (best.character_id >= 0) {
        result.character_id = best.character_id;
        result.character = best.character_name;
        result.confidence = best.confidence;
    }

    return result;
}

RecognitionOutput Pipeline::process(const cv::Mat& input_image) {
    RecognitionOutput output;
    auto t_start = std::chrono::steady_clock::now();

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
    auto t_end = std::chrono::steady_clock::now();
    output.elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    return output;
}

PieceResult Pipeline::process_single(const cv::Mat& input_image,
                                     std::string* error_message,
                                     double* elapsed_ms) {
    auto t_start = std::chrono::steady_clock::now();
    PieceResult result;

    if (error_message) error_message->clear();
    if (input_image.empty()) {
        if (error_message) *error_message = "Empty input image";
        return result;
    }
    if (!is_ready()) {
        if (error_message) *error_message = "Template library not loaded";
        return result;
    }

    auto preprocessed = preprocessor_.process(input_image);
    auto piece = detector_.detect_single(preprocessed.gray, preprocessed.color);
    result = process_single_piece(piece.roi_color, piece.roi_gray,
                                  piece.bounding_box);

    if (result.character_id < 0 && error_message) {
        *error_message = "Character recognition failed";
    }

    auto t_end = std::chrono::steady_clock::now();
    if (elapsed_ms) {
        *elapsed_ms =
            std::chrono::duration<double, std::milli>(t_end - t_start).count();
    }
    return result;
}

} // namespace junqi
