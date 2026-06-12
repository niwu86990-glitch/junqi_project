#include "junqi/pipeline.h"
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <vector>

namespace fs = std::filesystem;

struct GroundTruth {
    std::string left_color;
    std::string left_char;
    std::string right_color;
    std::string right_char;
};

static const std::map<int, std::string> ID_TO_NAME = {
    {1, "司令"}, {2, "军长"}, {3, "师长"}, {4, "旅长"},
    {5, "团长"}, {6, "营长"}, {7, "连长"}, {8, "排长"},
    {9, "工兵"}, {10, "地雷"}, {11, "炸弹"}, {12, "军旗"}
};

static bool load_ground_truth(const std::string& yaml_path, GroundTruth& gt) {
    std::ifstream file(yaml_path);
    if (!file.is_open()) return false;

    std::string line;
    std::string current_section;
    while (std::getline(file, line)) {
        if (line.find("left:") != std::string::npos) {
            current_section = "left";
        } else if (line.find("right:") != std::string::npos) {
            current_section = "right";
        } else if (current_section == "left" && line.find("color:") != std::string::npos) {
            auto pos = line.find("color:") + 6;
            gt.left_color = line.substr(pos);
            // trim
            gt.left_color.erase(0, gt.left_color.find_first_not_of(" \t"));
            gt.left_color.erase(gt.left_color.find_last_not_of(" \t") + 1);
        } else if (current_section == "left" && line.find("character:") != std::string::npos) {
            auto pos = line.find("character:") + 10;
            gt.left_char = line.substr(pos);
            gt.left_char.erase(0, gt.left_char.find_first_not_of(" \"\t"));
            gt.left_char.erase(gt.left_char.find_last_not_of(" \"\t") + 1);
        } else if (current_section == "right" && line.find("color:") != std::string::npos) {
            auto pos = line.find("color:") + 6;
            gt.right_color = line.substr(pos);
            gt.right_color.erase(0, gt.right_color.find_first_not_of(" \t"));
            gt.right_color.erase(gt.right_color.find_last_not_of(" \t") + 1);
        } else if (current_section == "right" && line.find("character:") != std::string::npos) {
            auto pos = line.find("character:") + 10;
            gt.right_char = line.substr(pos);
            gt.right_char.erase(0, gt.right_char.find_first_not_of(" \"\t"));
            gt.right_char.erase(gt.right_char.find_last_not_of(" \"\t") + 1);
        }
    }
    return !gt.left_color.empty() && !gt.right_color.empty();
}

static std::string color_name(junqi::PieceColor c) {
    switch (c) {
        case junqi::PieceColor::RED:   return "red";
        case junqi::PieceColor::BLACK: return "black";
        default: return "unknown";
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: benchmark <test_dir> <template_dir>\n";
        std::cerr << "  test_dir should contain scene_*.jpg and scene_*.yaml pairs\n";
        return 1;
    }

    std::string test_dir = argv[1];
    std::string template_dir = argv[2];

    junqi::Pipeline::Config config;
    config.template_dir = template_dir;

    junqi::Pipeline pipeline(config);
    if (!pipeline.is_ready()) {
        std::cerr << "Failed to load template library\n";
        return 1;
    }

    int total = 0;
    int correct_color = 0, correct_char = 0, fully_correct = 0;

    // Confusion matrix: 12x12
    std::vector<std::vector<int>> confusion(13, std::vector<int>(13, 0));

    std::vector<double> times_ms;

    for (const auto& entry : fs::directory_iterator(test_dir)) {
        std::string path = entry.path().string();
        if (path.size() < 4 || path.substr(path.size() - 4) != ".jpg") continue;

        std::string yaml_path = path.substr(0, path.size() - 4) + ".yaml";
        if (!fs::exists(yaml_path)) {
            std::cerr << "Skipping " << path << " (no matching .yaml)\n";
            continue;
        }

        GroundTruth gt;
        if (!load_ground_truth(yaml_path, gt)) {
            std::cerr << "Failed to parse: " << yaml_path << "\n";
            continue;
        }

        cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
        if (image.empty()) {
            std::cerr << "Failed to load: " << path << "\n";
            continue;
        }

        auto result = pipeline.process(image);
        total++;
        times_ms.push_back(result.elapsed_ms);

        bool l_color_ok = (color_name(result.left_piece.color) == gt.left_color);
        bool r_color_ok = (color_name(result.right_piece.color) == gt.right_color);
        bool l_char_ok = (result.left_piece.character == gt.left_char);
        bool r_char_ok = (result.right_piece.character == gt.right_char);

        if (l_color_ok) correct_color++;
        if (r_color_ok) correct_color++;
        if (l_char_ok) correct_char++;
        if (r_char_ok) correct_char++;
        if (l_color_ok && r_color_ok && l_char_ok && r_char_ok) fully_correct++;

        // Confusion matrix entries
        int gt_l_id = -1, pred_l_id = result.left_piece.character_id;
        int gt_r_id = -1, pred_r_id = result.right_piece.character_id;
        for (const auto& [id, name] : ID_TO_NAME) {
            if (name == gt.left_char) gt_l_id = id;
            if (name == gt.right_char) gt_r_id = id;
        }
        if (gt_l_id >= 1 && pred_l_id >= 1) confusion[gt_l_id][pred_l_id]++;
        if (gt_r_id >= 1 && pred_r_id >= 1) confusion[gt_r_id][pred_r_id]++;

        std::cout << path << ": "
                  << (l_color_ok ? "O" : "X") << (l_char_ok ? "O" : "X")
                  << " / "
                  << (r_color_ok ? "O" : "X") << (r_char_ok ? "O" : "X")
                  << " (" << result.elapsed_ms << "ms)\n";
    }

    // Summary
    int total_pieces = total * 2;
    std::cout << "\n========================================\n";
    std::cout << "  Benchmark Results\n";
    std::cout << "========================================\n";
    std::cout << "Total images:    " << total << "\n";
    std::cout << "Color accuracy:  " << std::fixed << std::setprecision(1)
              << (100.0 * correct_color / total_pieces) << "% ("
              << correct_color << "/" << total_pieces << ")\n";
    std::cout << "Char accuracy:   " << std::fixed << std::setprecision(1)
              << (100.0 * correct_char / total_pieces) << "% ("
              << correct_char << "/" << total_pieces << ")\n";
    std::cout << "End-to-end:      " << std::fixed << std::setprecision(1)
              << (100.0 * fully_correct / total) << "% ("
              << fully_correct << "/" << total << ")\n";

    double avg_time = 0;
    for (double t : times_ms) avg_time += t;
    avg_time /= times_ms.size();
    std::cout << "Avg time:        " << std::fixed << std::setprecision(1)
              << avg_time << " ms\n";

    // Confusion matrix
    std::cout << "\nConfusion Matrix (rows=ground truth, cols=predicted):\n";
    std::cout << "        ";
    for (int j = 1; j <= 12; j++) std::cout << std::setw(4) << j;
    std::cout << "\n";
    for (int i = 1; i <= 12; i++) {
        std::cout << std::setw(4) << i << " " << ID_TO_NAME.at(i) << ":";
        for (int j = 1; j <= 12; j++) {
            std::cout << std::setw(4) << confusion[i][j];
        }
        std::cout << "\n";
    }

    
    return 0;
}
