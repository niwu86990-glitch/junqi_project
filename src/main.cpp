#include "junqi/pipeline.h"
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cstdlib>

static const char* color_str(junqi::PieceColor c) {
    switch (c) {
        case junqi::PieceColor::RED:   return "RED";
        case junqi::PieceColor::BLACK: return "BLACK";
        default: return "UNKNOWN";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: junqi_cli <image_path> [template_dir]\n";
        return 1;
    }

    std::string image_path = argv[1];
    std::string template_dir = (argc >= 3) ? argv[2] : "templates/";

    cv::Mat image = cv::imread(image_path, cv::IMREAD_COLOR);
    if (image.empty()) {
        std::cerr << "Failed to load image: " << image_path << "\n";
        return 1;
    }

    junqi::Pipeline::Config config;
    config.template_dir = template_dir;
    config.verbose = true;

    junqi::Pipeline pipeline(config);

    if (!pipeline.is_ready()) {
        std::cerr << "Failed to load template library from: " << template_dir << "\n";
        return 1;
    }

    auto result = pipeline.process(image);

    std::cout << "========================================\n";
    std::cout << "  Junqi Recognition Result\n";
    std::cout << "========================================\n";

    if (result.success) {
        std::cout << "Left  piece: [" << color_str(result.left_piece.color)
                  << "] " << result.left_piece.character
                  << " (conf: " << result.left_piece.confidence << ")\n";
        std::cout << "Right piece: [" << color_str(result.right_piece.color)
                  << "] " << result.right_piece.character
                  << " (conf: " << result.right_piece.confidence << ")\n";
    } else {
        std::cout << "Recognition failed: " << result.error_message << "\n";
    }

    std::cout << "Elapsed: " << result.elapsed_ms << " ms\n";

    return result.success ? 0 : 1;
}
