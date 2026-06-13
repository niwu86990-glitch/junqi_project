#pragma once

#include <string>
#include <opencv2/core.hpp>

namespace junqi {

struct PieceResult {
    std::string character;
    int character_id = -1;
    float confidence = 0.0f;
    cv::Rect bounding_box;
};

struct RecognitionOutput {
    PieceResult left_piece;
    PieceResult right_piece;
    bool success = false;
    std::string error_message;
    double elapsed_ms = 0.0;
};

} // namespace junqi
