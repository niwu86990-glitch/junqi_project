#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <cmath>
#include <cstring>

static double score_template(const cv::Mat& norm64) {
    int total = 64 * 64;
    int white = cv::countNonZero(norm64);
    double ratio = static_cast<double>(white) / total;
    if (ratio < 0.04 || ratio > 0.78) return -1.0;

    int border_white = 0;
    for (int r = 0; r < 64; r++) {
        const uchar* row = norm64.ptr<uchar>(r);
        for (int c = 0; c < 64; c++) {
            if (row[c] && (r < 5 || r >= 59 || c < 5 || c >= 59))
                border_white++;
        }
    }
    double border_ratio = static_cast<double>(border_white) / std::max(1, white);
    if (border_ratio > 0.30) return -1.0;

    double spread_score = 1.0 - border_ratio * 3.0;
    double ratio_score = 1.0 - std::abs(ratio - 0.35) * 2.5;
    return (spread_score + ratio_score) / 2.0;
}

// POSIX 替代：获取文件扩展名
static std::string file_extension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    return path.substr(dot);
}

int main(int argc, char** argv) {
    bool do_delete = false;
    std::string dir;
    int top_n = 0;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--delete") do_delete = true;
        else if (dir.empty()) dir = a;
        else top_n = std::stoi(a);
    }

    if (dir.empty()) {
        std::cerr << "Usage: score_templates [--delete] <template_dir> [top_n]\n";
        return 1;
    }

    struct Entry { std::string path; double score; int white; };

    // POSIX 替代 fs::directory_iterator
    DIR* top = opendir(dir.c_str());
    if (!top) {
        std::cerr << "Cannot open directory: " << dir << "\n";
        return 1;
    }

    struct dirent* d;
    while ((d = readdir(top)) != nullptr) {
        if (std::strcmp(d->d_name, ".") == 0 ||
            std::strcmp(d->d_name, "..") == 0) continue;

        std::string name(d->d_name);

        // 检查是否为目录
        std::string subdir = dir + "/" + name;
        struct stat st;
        if (stat(subdir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        std::vector<Entry> entries;

        DIR* sub = opendir(subdir.c_str());
        if (!sub) continue;

        struct dirent* f;
        while ((f = readdir(sub)) != nullptr) {
            std::string fname(f->d_name);
            if (file_extension(fname) != ".png") continue;

            std::string full_path = subdir + "/" + fname;
            cv::Mat img = cv::imread(full_path, cv::IMREAD_GRAYSCALE);
            if (img.empty() || img.cols != 64 || img.rows != 64) continue;

            double s = score_template(img);
            Entry e;
            e.path  = full_path;
            e.score = s;
            e.white = cv::countNonZero(img);
            entries.push_back(e);
        }
        closedir(sub);

        std::sort(entries.begin(), entries.end(),
                  [](const Entry& a, const Entry& b) { return a.score > b.score; });

        int n = (top_n > 0) ? std::min(top_n, static_cast<int>(entries.size())) : static_cast<int>(entries.size());

        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            const char* marker = (i < n) ? "KEEP" : "DEL ";
            if (do_delete && i >= n) {
                remove(entries[i].path.c_str());  // POSIX remove()
                marker = "DELETED";
            }

            // 提取文件名
            auto slash = entries[i].path.rfind('/');
            std::string filename = (slash != std::string::npos)
                ? entries[i].path.substr(slash + 1) : entries[i].path;

            std::cout << "  " << marker << " " << std::setw(4) << (i + 1) << ". "
                      << filename
                      << "  score=" << entries[i].score
                      << " white=" << entries[i].white << "/4096\n";
        }
    }
    closedir(top);
    return 0;
}
