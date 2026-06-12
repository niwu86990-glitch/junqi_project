#include "junqi/pipeline.h"
#include "junqi/template_library.h"
#include "junqi/preprocessor.h"
#include "junqi/detector.h"
#include "junqi/character_extractor.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <vector>

namespace {

struct Scenario {
    cv::Scalar background;
    cv::Scalar ink;
    cv::Point center;
    double angle;
    bool red;
    bool gradient;
    bool glare;
    bool texture;
};

cv::Mat make_scene(const cv::Mat& character, const Scenario& scenario) {
    const cv::Size scene_size(640, 480);
    cv::Mat scene(scene_size, CV_8UC3, scenario.background);

    if (scenario.texture) {
        for (int y = 0; y < scene.rows; y += 18) {
            cv::line(scene, cv::Point(0, y), cv::Point(scene.cols - 1, y + 5),
                     cv::Scalar(95, 120, 145), 2);
        }
    }

    const cv::Size piece_size(300, 180);
    cv::Mat piece(piece_size, CV_8UC3, cv::Scalar(205, 215, 225));
    cv::rectangle(piece, cv::Rect(3, 3, piece.cols - 6, piece.rows - 6),
                  cv::Scalar(130, 140, 150), 4);

    cv::Mat char_mask;
    cv::resize(character, char_mask, cv::Size(160, 150), 0, 0,
               cv::INTER_NEAREST);
    cv::threshold(char_mask, char_mask, 128, 255, cv::THRESH_BINARY);
    cv::Rect char_box((piece.cols - char_mask.cols) / 2,
                      (piece.rows - char_mask.rows) / 2,
                      char_mask.cols, char_mask.rows);
    piece(char_box).setTo(
        scenario.ink,
        char_mask);

    cv::Mat piece_mask(piece_size, CV_8UC1, cv::Scalar(255));
    cv::Mat transform =
        cv::getRotationMatrix2D(cv::Point2f(piece.cols / 2.0f,
                                           piece.rows / 2.0f),
                               scenario.angle, 1.0);
    transform.at<double>(0, 2) +=
        scenario.center.x - piece.cols / 2.0;
    transform.at<double>(1, 2) +=
        scenario.center.y - piece.rows / 2.0;

    cv::Mat warped_piece(scene_size, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat warped_mask(scene_size, CV_8UC1, cv::Scalar(0));
    cv::warpAffine(piece, warped_piece, transform, scene_size,
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    cv::warpAffine(piece_mask, warped_mask, transform, scene_size,
                   cv::INTER_NEAREST, cv::BORDER_CONSTANT);
    warped_piece.copyTo(scene, warped_mask);

    if (scenario.glare) {
        cv::Mat overlay = scene.clone();
        cv::ellipse(overlay,
                    scenario.center + cv::Point(45, -25),
                    cv::Size(70, 38), scenario.angle, 0, 360,
                    cv::Scalar(255, 255, 255), cv::FILLED);
        cv::addWeighted(overlay, 0.58, scene, 0.42, 0.0, scene);
    }

    if (scenario.gradient) {
        cv::Mat float_scene;
        scene.convertTo(float_scene, CV_32FC3);
        for (int x = 0; x < scene.cols; ++x) {
            const float factor = 0.55f + 0.75f * x / scene.cols;
            float_scene.col(x) *= factor;
        }
        float_scene.convertTo(scene, CV_8UC3);
    }

    cv::GaussianBlur(scene, scene, cv::Size(3, 3), 0.6);
    return scene;
}

const char* color_name(junqi::PieceColor color) {
    switch (color) {
        case junqi::PieceColor::RED: return "red";
        case junqi::PieceColor::BLACK: return "black";
        default: return "unknown";
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string template_dir = argc >= 2 ? argv[1] : "templates";

    junqi::TemplateLibrary library;
    if (!library.load(template_dir)) {
        std::cerr << "Failed to load templates from " << template_dir << "\n";
        return 1;
    }

    junqi::Pipeline::Config config;
    config.template_dir = template_dir;
    junqi::Pipeline pipeline(config);
    if (!pipeline.is_ready()) return 1;

    const std::vector<Scenario> scenarios = {
        {cv::Scalar(10, 10, 10), cv::Scalar(25, 25, 25),
         cv::Point(320, 240), 0.0,
         false, false, false, false},
        {cv::Scalar(190, 190, 190), cv::Scalar(25, 35, 205),
         cv::Point(235, 205), 7.0,
         true, true, true, false},
        {cv::Scalar(75, 100, 125), cv::Scalar(25, 35, 205),
         cv::Point(405, 275), -8.0,
         true, false, false, true},
        {cv::Scalar(225, 215, 195), cv::Scalar(25, 25, 25),
         cv::Point(330, 225), 5.0,
         false, true, false, true},
        // Low-saturation red caused by overexposure/white balance.
        {cv::Scalar(185, 195, 205), cv::Scalar(115, 130, 180),
         cv::Point(310, 235), -4.0,
         true, false, false, false},
        // Pale reflective red with a broad white highlight.
        {cv::Scalar(165, 175, 190), cv::Scalar(150, 165, 210),
         cv::Point(350, 245), 6.0,
         true, true, true, false},
        // Underexposed red must not collapse into black.
        {cv::Scalar(35, 40, 45), cv::Scalar(20, 25, 105),
         cv::Point(285, 255), -6.0,
         true, true, false, true}
    };

    int total = 0;
    int character_correct = 0;
    int color_correct = 0;

    for (const auto& character : library.templates()) {
        if (character.samples.empty()) continue;
        for (size_t i = 0; i < scenarios.size(); ++i) {
            cv::Mat scene = make_scene(character.samples.front(),
                                       scenarios[i]);
            if (character.character_id == 1 ||
                character.character_id == 5 ||
                character.character_id == 8) {
                const std::string debug_prefix =
                    "/tmp/junqi_robust_" +
                    std::to_string(character.character_id) + "_";
                cv::imwrite("/tmp/junqi_robust_scene_" +
                            std::to_string(i) + ".png", scene);
                junqi::Preprocessor preprocessor;
                junqi::Detector detector;
                junqi::CharacterExtractor extractor;
                auto preprocessed = preprocessor.process(scene);
                auto detected =
                    detector.detect_single(preprocessed.gray,
                                           preprocessed.color);
                cv::imwrite(debug_prefix + "roi_" +
                            std::to_string(i) + ".png",
                            detected.roi_color);
                auto candidates = extractor.extract_candidates(
                    detected.roi_color, detected.roi_gray);
                for (size_t candidate_index = 0;
                     candidate_index < candidates.size();
                     ++candidate_index) {
                    cv::imwrite(
                        debug_prefix + "candidate_" +
                        std::to_string(i) + "_" +
                        std::to_string(candidate_index) + ".png",
                        extractor.normalize(candidates[candidate_index],
                                            cv::Size(64, 64)));
                }
            }
            std::string error;
            double elapsed = 0.0;
            junqi::PieceResult result =
                pipeline.process_single(scene, &error, &elapsed);

            const bool char_ok =
                result.character_id == character.character_id;
            const junqi::PieceColor expected_color =
                scenarios[i].red ? junqi::PieceColor::RED
                                 : junqi::PieceColor::BLACK;
            const bool color_ok = result.color == expected_color;
            total++;
            if (char_ok) character_correct++;
            if (color_ok) color_correct++;

            std::cout << character.chinese_name << " scenario " << i
                      << ": predicted=" << result.character
                      << " confidence=" << result.confidence
                      << " color=" << color_name(result.color)
                      << " bbox=" << result.bounding_box
                      << " time=" << elapsed << "ms "
                      << (char_ok ? "CHAR_OK " : "CHAR_FAIL ")
                      << (color_ok ? "COLOR_OK" : "COLOR_FAIL")
                      << "\n";
        }
    }

    const double char_accuracy =
        total ? 100.0 * character_correct / total : 0.0;
    const double color_accuracy =
        total ? 100.0 * color_correct / total : 0.0;
    std::cout << "\nSynthetic robustness: character=" << char_accuracy
              << "% color=" << color_accuracy << "% (" << total
              << " cases)\n";

    return (char_accuracy >= 80.0 && color_accuracy >= 90.0) ? 0 : 2;
}
