#include "junqi/template_library.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstring>
#include <cctype>

namespace junqi {

bool TemplateLibrary::load(const std::string& template_dir) {
    templates_.clear();
    id_to_name_.clear();

    // Known character name mapping
    std::map<int, std::string> known = {
        {1, "司令"}, {2, "军长"}, {3, "师长"}, {4, "旅长"},
        {5, "团长"}, {6, "营长"}, {7, "连长"}, {8, "排长"},
        {9, "工兵"}, {10, "地雷"}, {11, "炸弹"}, {12, "军旗"}
    };

    // 检查目录是否存在（POSIX stat）
    struct stat st;
    if (stat(template_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        return false;
    }

    // 遍历一级子目录（POSIX opendir/readdir）
    DIR* dir = opendir(template_dir.c_str());
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过 . 和 ..
        if (std::strcmp(entry->d_name, ".") == 0 ||
            std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string dir_name(entry->d_name);

        // 检查是否为目录
        if (entry->d_type != DT_DIR) {
            // d_type 可能为 DT_UNKNOWN（某些文件系统），用 stat 回退
            std::string full_path = template_dir + "/" + dir_name;
            if (stat(full_path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                continue;
            }
        }

        // 【替换正则：查找第一个下划线，前面就是数字ID】
        size_t underline_pos = dir_name.find('_');
        if (underline_pos == std::string::npos || underline_pos == 0)
        {
            continue;
        }
        std::string num_str = dir_name.substr(0, underline_pos);
        // 校验截取部分全部是数字
        bool all_digit = true;
        for (char ch : num_str)
        {
            if (!std::isdigit(static_cast<unsigned char>(ch)))
            {
                all_digit = false;
                break;
            }
        }
        if (!all_digit)
            continue;
        int char_id = std::stoi(num_str);

        id_to_name_[char_id] = known[char_id];

        CharacterTemplate tmpl;
        tmpl.character_id = char_id;
        tmpl.chinese_name = known[char_id];

        // 遍历二级子目录中的模板图片
        std::string sub_path = template_dir + "/" + dir_name;
        DIR* subdir = opendir(sub_path.c_str());
        if (!subdir) continue;

        struct dirent* file;
        while ((file = readdir(subdir)) != nullptr) {
            if (file->d_type != DT_REG && file->d_type != DT_UNKNOWN) continue;

            std::string fname(file->d_name);

            // 提取扩展名并转小写
            auto dot_pos = fname.rfind('.');
            if (dot_pos == std::string::npos) continue;

            std::string ext = fname.substr(dot_pos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") continue;

            std::string img_path = sub_path + "/" + fname;
            cv::Mat img = cv::imread(img_path, cv::IMREAD_GRAYSCALE);
            if (img.empty()) continue;

            if (img.size() != target_size_) {
                cv::resize(img, img, target_size_, 0, 0, cv::INTER_AREA);
            }
            tmpl.samples.push_back(img);
        }
        closedir(subdir);

        if (!tmpl.samples.empty()) {
            templates_.push_back(tmpl);
        }
    }
    closedir(dir);

    std::sort(templates_.begin(), templates_.end(),
              [](const CharacterTemplate& a, const CharacterTemplate& b) {
                  return a.character_id < b.character_id;
              });

    return !templates_.empty();
}

const std::string& TemplateLibrary::name_of(int id) const {
    static const std::string unknown = "UNKNOWN";
    auto it = id_to_name_.find(id);
    return it != id_to_name_.end() ? it->second : unknown;
}

} // namespace junqi
