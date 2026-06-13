#include "junqi/character_extractor.h"
#include "junqi/detector.h"
#include "junqi/pipeline.h"
#include "junqi/preprocessor.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool has_jpeg_extension(const std::string& name) {
    return name.size() >= 4 &&
           (name.substr(name.size() - 4) == ".jpg" ||
            name.substr(name.size() - 5) == ".jpeg");
}

std::string basename_without_extension(const std::string& name) {
    const size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: analyze_real_samples <input_dir> "
                     "<template_dir> <output_dir>\n";
        return 1;
    }

    const std::string input_dir = argv[1];
    const std::string template_dir = argv[2];
    const std::string output_dir = argv[3];
    mkdir(output_dir.c_str(), 0755);

    junqi::Pipeline::Config config;
    config.template_dir = template_dir;
    junqi::Pipeline pipeline(config);
    junqi::Preprocessor preprocessor;
    junqi::Detector detector;
    junqi::CharacterExtractor extractor;

    DIR* dir = opendir(input_dir.c_str());
    if (!dir) return 1;

    std::vector<std::string> files;
    while (dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (has_jpeg_extension(name)) files.push_back(name);
    }
    closedir(dir);
    std::sort(files.begin(), files.end());

    for (const std::string& name : files) {
        const std::string path = input_dir + "/" + name;
        const std::string prefix =
            output_dir + "/" + basename_without_extension(name);
        const cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
        if (image.empty()) continue;

        const auto preprocessed = preprocessor.process(image);
        const auto piece =
            detector.detect_single(preprocessed.gray, preprocessed.color);
        cv::Mat marked = image.clone();
        cv::rectangle(marked, piece.bounding_box, cv::Scalar(0, 255, 0), 2);
        cv::imwrite(prefix + "_detected.jpg", marked);
        cv::imwrite(prefix + "_roi.jpg", piece.roi_color);

        const auto candidates =
            extractor.extract_candidates(piece.roi_color, piece.roi_gray);
        for (size_t i = 0; i < candidates.size(); ++i) {
            cv::imwrite(prefix + "_candidate_" + std::to_string(i) + ".png",
                        extractor.normalize(candidates[i], cv::Size(64, 64)));
        }

        std::string error;
        double elapsed_ms = 0.0;
        const auto result =
            pipeline.process_single(image, &error, &elapsed_ms);
        std::cout << name << " bbox=" << piece.bounding_box
                  << " candidates=" << candidates.size()
                  << " predicted=" << result.character
                  << " confidence=" << result.confidence
                  << " elapsed=" << elapsed_ms << "ms\n";
    }
    return 0;
}
