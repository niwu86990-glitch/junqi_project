#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <map>

namespace junqi {

struct CharacterTemplate {
    int character_id;
    std::string chinese_name;
    std::vector<cv::Mat> samples;
};

class TemplateLibrary {
public:
    bool load(const std::string& template_dir);    //有点问题

    const std::vector<CharacterTemplate>& templates() const { return templates_; }

    const std::string& name_of(int id) const;

    cv::Size target_size() const { return target_size_; }

    size_t size() const { return templates_.size(); }

private:
    std::vector<CharacterTemplate> templates_;
    std::map<int, std::string> id_to_name_;
    cv::Size target_size_{64, 64};
};

} // namespace junqi
