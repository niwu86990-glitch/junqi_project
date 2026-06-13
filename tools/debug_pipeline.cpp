#include "junqi/preprocessor.h"
#include "junqi/detector.h"
#include "junqi/character_extractor.h"
#include "junqi/recognizer.h"
#include "junqi/template_library.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <algorithm>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: debug_pipeline <image> <template_dir>\n";
        return 1;
    }

    cv::Mat image = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (image.empty()) { std::cerr << "Failed to load\n"; return 1; }
    std::cout << "Image: " << image.cols << "x" << image.rows << "\n";

    junqi::Preprocessor preprocessor;
    auto pre = preprocessor.process(image);

    junqi::Detector detector;
    auto pieces = detector.detect(pre.gray, pre.color);
    std::cout << "Detected pieces: " << pieces.size() << "\n";
    for (size_t i = 0; i < pieces.size(); i++) {
        auto& p = pieces[i];
        std::cout << "  piece " << i << ": bbox=" << p.bounding_box
                  << " is_left=" << p.is_left << "\n";
    }

    if (pieces.size() < 2) {
        std::cerr << "Need >= 2 pieces\n";
        return 1;
    }

    junqi::CharacterExtractor extractor;
    junqi::TemplateLibrary lib;
    lib.load(argv[2]);
    junqi::Recognizer recognizer(lib);

    for (size_t i = 0; i < pieces.size(); i++) {
        auto& p = pieces[i];
        std::cout << "\n--- Piece " << i << " (" << (p.is_left ? "L" : "R") << ") ---\n";

        cv::Mat char_bin = extractor.extract(p.roi_gray);
        if (char_bin.empty()) {
            std::cout << "Char extract: FAILED\n";
            continue;
        }
        cv::Mat norm = extractor.normalize(char_bin, cv::Size(64, 64));

        auto rec = recognizer.recognize(norm);
        std::cout << "Best: " << rec.character_name << " (id=" << rec.character_id
                  << ") conf=" << rec.confidence << "\n";

        // Top-K scores for all characters
        struct Score { int id; std::string name; float conf; };
        std::vector<Score> scores;
        const double rot_start = -4.0, rot_end = 4.0, rot_step = 0.5;
        const double scale_start = 0.92, scale_end = 1.08, scale_step = 0.04;
        for (const auto& tmpl : lib.templates()) {
            float best = 0;
            for (const auto& sample : tmpl.samples) {
                for (double ang = rot_start; ang <= rot_end; ang += rot_step) {
                    cv::Point2f c(sample.cols/2.f, sample.rows/2.f);
                    cv::Mat rot = cv::getRotationMatrix2D(c, ang, 1.0);
                    cv::Mat rotated;
                    cv::warpAffine(sample, rotated, rot, sample.size(),
                                   cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(255));
                    for (double s = scale_start; s <= scale_end; s += scale_step) {
                        cv::Mat scaled;
                        cv::resize(rotated, scaled, cv::Size(), s, s, cv::INTER_AREA);
                        // Ensure image >= template for matchTemplate
                        cv::Mat img, tpl;
                        if (norm.rows >= scaled.rows && norm.cols >= scaled.cols) {
                            img = norm; tpl = scaled;
                        } else {
                            int pr = scaled.rows - norm.rows;
                            int pc = scaled.cols - norm.cols;
                            cv::copyMakeBorder(norm, img, 0, std::max(0, pr), 0, std::max(0, pc),
                                               cv::BORDER_CONSTANT, cv::Scalar(255));
                            tpl = scaled;
                        }
                        cv::Mat result_mat;
                        cv::matchTemplate(img, tpl, result_mat, cv::TM_CCOEFF_NORMED);
                        double minVal, maxVal;
                        cv::minMaxLoc(result_mat, &minVal, &maxVal);
                        if (maxVal > best) best = static_cast<float>(maxVal);
                    }
                }
            }
            scores.push_back({tmpl.character_id, tmpl.chinese_name, best});
        }
        std::sort(scores.begin(), scores.end(),
                  [](const Score& a, const Score& b) { return a.conf > b.conf; });
        std::cout << "Top-5: ";
        for (int k = 0; k < 5 && k < (int)scores.size(); k++) {
            std::cout << scores[k].name << "(" << scores[k].conf << ") ";
        }
        std::cout << "\n";

        cv::imwrite("/tmp/debug_piece_" + std::to_string(i) + "_roi.png", p.roi_color);
        cv::imwrite("/tmp/debug_piece_" + std::to_string(i) + "_char.png", norm);
        std::cout << "Saved debug images to /tmp/\n";
    }

    return 0;
}
