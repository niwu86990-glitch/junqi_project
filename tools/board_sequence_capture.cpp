#include "junqi/camera_capture.h"
#include "junqi/pipeline.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

struct TestCase {
    int expected_id;
    const char* expected_name;
    const char* expected_slug;
    int repetition;
};

const char* piece_slug(int id) {
    static const char* slugs[] = {
        "unknown", "siling", "junzhang", "shizhang", "lvzhang",
        "tuanzhang", "yingzhang", "lianzhang", "paizhang", "gongbing",
        "dilei", "zhadan", "junqi"
    };
    return id >= 1 && id <= 12 ? slugs[id] : slugs[0];
}

bool ensure_directory(const std::string& path) {
    if (::mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) return true;
    std::perror("mkdir");
    return false;
}

void wait_for_capture(int seconds, const TestCase& test, int index, int total) {
    std::printf(
        "\n[%02d/%02d] 请放置第 %d 枚%s，%d 秒后拍照。\n",
        index, total, test.repetition, test.expected_name, seconds);
    std::fflush(stdout);

    for (int remaining = seconds; remaining > 0; --remaining) {
        if (remaining == seconds || remaining <= 5 || remaining % 5 == 0) {
            std::printf("  倒计时: %d 秒\n", remaining);
            std::fflush(stdout);
        }
        ::sleep(1);
    }
}

std::string make_filename(const std::string& output_dir, int index,
                          const TestCase& test,
                          const junqi::PieceResult& result) {
    const int confidence =
        static_cast<int>(result.confidence * 1000.0f + 0.5f);
    std::ostringstream filename;
    filename << output_dir << "/sample_"
             << std::setfill('0') << std::setw(2) << index
             << "_expected_" << std::setw(2) << test.expected_id
             << "_" << test.expected_slug
             << "_pred_" << std::setw(2)
             << (result.character_id >= 0 ? result.character_id : 0)
             << "_" << piece_slug(result.character_id)
             << "_conf_" << std::setw(3) << confidence
             << ".jpg";
    return filename.str();
}

} // namespace

int main(int argc, char** argv) {
    const std::string template_dir =
        argc >= 2 ? argv[1] : "./templates";
    const std::string output_dir =
        argc >= 3 ? argv[2] : "/root/junqi_test_samples";
    const char* camera_device = argc >= 4 ? argv[3] : "/dev/video0";
    const int interval_seconds =
        argc >= 5 ? std::max(1, std::atoi(argv[4])) : 30;

    const std::vector<TestCase> tests = {
        {2, "军长", "junzhang", 1},
        {2, "军长", "junzhang", 2},
        {2, "军长", "junzhang", 3},
        {8, "排长", "paizhang", 1},
        {8, "排长", "paizhang", 2},
        {8, "排长", "paizhang", 3},
        {11, "炸弹", "zhadan", 1},
        {11, "炸弹", "zhadan", 2},
        {11, "炸弹", "zhadan", 3},
        {12, "军旗", "junqi", 1},
        {12, "军旗", "junqi", 2},
        {12, "军旗", "junqi", 3},
    };

    if (!ensure_directory(output_dir)) return 1;

    junqi::Pipeline::Config config;
    config.template_dir = template_dir;
    junqi::Pipeline pipeline(config);
    if (!pipeline.is_ready()) {
        std::fprintf(stderr, "模板库加载失败: %s\n", template_dir.c_str());
        return 1;
    }

    junqi::CameraCapture camera;
    if (!camera.open(camera_device)) {
        std::fprintf(stderr, "摄像头打开失败: %s\n", camera_device);
        return 1;
    }

    const std::string csv_path = output_dir + "/results.csv";
    std::ofstream csv(csv_path.c_str(), std::ios::out | std::ios::trunc);
    if (!csv) {
        std::fprintf(stderr, "无法创建结果文件: %s\n", csv_path.c_str());
        return 1;
    }
    csv << "index,expected_id,expected_name,repetition,predicted_id,"
           "predicted_name,confidence,elapsed_ms,image\n";

    std::printf("测试开始，共 12 次，每次间隔 %d 秒。\n", interval_seconds);
    std::printf("顺序：军长 x3 -> 排长 x3 -> 炸弹 x3 -> 军旗 x3\n");
    std::printf("输出目录：%s\n", output_dir.c_str());

    int saved = 0;
    for (size_t i = 0; i < tests.size(); ++i) {
        const TestCase& test = tests[i];
        const int index = static_cast<int>(i) + 1;
        wait_for_capture(interval_seconds, test, index,
                         static_cast<int>(tests.size()));

        cv::Mat frame;
        if (!camera.capture(frame) || frame.empty()) {
            std::fprintf(stderr, "[%02d] 截图失败，继续下一次。\n", index);
            csv << index << "," << test.expected_id << ","
                << test.expected_name << "," << test.repetition
                << ",-1,capture_failed,0,0,\n";
            csv.flush();
            continue;
        }

        std::string error;
        double elapsed_ms = 0.0;
        const junqi::PieceResult result =
            pipeline.process_single(frame, &error, &elapsed_ms);
        const std::string image_path =
            make_filename(output_dir, index, test, result);

        if (!cv::imwrite(image_path, frame)) {
            std::fprintf(stderr, "[%02d] 图片保存失败: %s\n",
                         index, image_path.c_str());
        } else {
            ++saved;
        }

        const std::string predicted_name =
            result.character.empty() ? "UNKNOWN" : result.character;
        csv << index << "," << test.expected_id << ","
            << test.expected_name << "," << test.repetition << ","
            << result.character_id << "," << predicted_name << ","
            << std::fixed << std::setprecision(4) << result.confidence << ","
            << std::setprecision(1) << elapsed_ms << ","
            << image_path.substr(output_dir.size() + 1) << "\n";
        csv.flush();

        std::printf(
            "[%02d] 预期=%s 识别=%s ID=%d 置信度=%.1f%% 耗时=%.0fms\n",
            index, test.expected_name, predicted_name.c_str(),
            result.character_id, result.confidence * 100.0f, elapsed_ms);
        std::printf("       已保存: %s\n", image_path.c_str());
        if (!error.empty()) {
            std::printf("       识别信息: %s\n", error.c_str());
        }
        std::fflush(stdout);
    }

    std::printf("\n测试结束：成功保存 %d/%d 张图片。\n",
                saved, static_cast<int>(tests.size()));
    std::printf("结果目录：%s\n", output_dir.c_str());
    std::printf("汇总文件：%s\n", csv_path.c_str());
    return saved == static_cast<int>(tests.size()) ? 0 : 2;
}
