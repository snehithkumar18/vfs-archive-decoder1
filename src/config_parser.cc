#include "config_parser.h"
#include "string_utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace PixelForge {

bool ConfigParser::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return LoadFromString(buffer.str());
}

bool ConfigParser::LoadFromString(const std::string& content) {
    Clear();
    std::vector<std::string> lines = StringUtils::Split(content, '\n');
    std::string current_section = "";

    for (std::string& line : lines) {
        line = StringUtils::Trim(line);
        if (line.empty()) continue;
        
        // Strip comment
        if (line[0] == ';' || line[0] == '#') continue;

        // Strip inline comment
        size_t comment_pos = line.find(';');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
            line = StringUtils::Trim(line);
        }
        comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
            line = StringUtils::Trim(line);
        }
        
        if (line.empty()) continue;

        // Parse section
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            current_section = StringUtils::Trim(current_section);
            continue;
        }

        // Parse key-value pair
        size_t equal_pos = line.find('=');
        if (equal_pos == std::string::npos) continue;

        std::string key = line.substr(0, equal_pos);
        std::string val = line.substr(equal_pos + 1);

        key = StringUtils::Trim(key);
        val = StringUtils::Trim(val);

        if (!key.empty()) {
            data_[current_section][key] = val;
        }
    }

    return true;
}

bool ConfigParser::HasSection(const std::string& section) const {
    return data_.find(section) != data_.end();
}

bool ConfigParser::HasKey(const std::string& section, const std::string& key) const {
    auto sec_it = data_.find(section);
    if (sec_it == data_.end()) return false;
    return sec_it->second.find(key) != sec_it->second.end();
}

std::string ConfigParser::GetString(const std::string& section, const std::string& key, const std::string& default_val) const {
    auto sec_it = data_.find(section);
    if (sec_it == data_.end()) return default_val;
    auto key_it = sec_it->second.find(key);
    if (key_it == sec_it->second.end()) return default_val;
    return key_it->second;
}

int ConfigParser::GetInt(const std::string& section, const std::string& key, int default_val) const {
    std::string val = GetString(section, key, "");
    if (val.empty()) return default_val;
    try {
        return std::stoi(val);
    } catch (...) {
        return default_val;
    }
}

double ConfigParser::GetDouble(const std::string& section, const std::string& key, double default_val) const {
    std::string val = GetString(section, key, "");
    if (val.empty()) return default_val;
    try {
        return std::stod(val);
    } catch (...) {
        return default_val;
    }
}

bool ConfigParser::GetBool(const std::string& section, const std::string& key, bool default_val) const {
    std::string val = GetString(section, key, "");
    if (val.empty()) return default_val;
    std::string lower_val = val;
    std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);
    if (lower_val == "true" || lower_val == "yes" || lower_val == "1" || lower_val == "on") return true;
    if (lower_val == "false" || lower_val == "no" || lower_val == "0" || lower_val == "off") return false;
    return default_val;
}

std::vector<std::string> ConfigParser::GetSections() const {
    std::vector<std::string> sections;
    for (const auto& pair : data_) {
        sections.push_back(pair.first);
    }
    return sections;
}

std::vector<std::string> ConfigParser::GetKeys(const std::string& section) const {
    std::vector<std::string> keys;
    auto sec_it = data_.find(section);
    if (sec_it != data_.end()) {
        for (const auto& pair : sec_it->second) {
            keys.push_back(pair.first);
        }
    }
    return keys;
}

void ConfigParser::Clear() {
    data_.clear();
}

} // namespace PixelForge
