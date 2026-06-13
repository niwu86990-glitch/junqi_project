#pragma once

#include "result.h"
#include "preprocessor.h"
#include "detector.h"
#include "character_extractor.h"
#include "recognizer.h"
#include "template_library.h"
#include <opencv2/core.hpp>
#include <string>

namespace junqi {

class Pipeline {
public:
    struct Config {
        std::string template_dir;
        float confidence_threshold = 0.70f;
        bool enable_rectification = true;
        bool verbose = false;
    };

    explicit Pipeline(const Config& config);

    bool is_ready() const;

    RecognitionOutput process(const cv::Mat& input_image);

    /// Recognize one piece placed under the camera.
    PieceResult process_single(const cv::Mat& input_image,
                               std::string* error_message = nullptr,
                               double* elapsed_ms = nullptr);

private:
    Config config_;
    Preprocessor preprocessor_;
    Detector detector_;
    CharacterExtractor character_extractor_;
    TemplateLibrary library_;
    Recognizer recognizer_;

    PieceResult process_single_piece(const cv::Mat& piece_color,
                                      const cv::Mat& piece_gray,
                                      const cv::Rect& bbox);
};

} // namespace junqi
