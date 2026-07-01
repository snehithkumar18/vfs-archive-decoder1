#include "config_parser.h"
#include "string_utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace PixelForge {

namespace {

std::string strip_inline_comment(const std::string& line) {
    bool in_quotes = false;
    bool escaped = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && (c == ';' || c == '#')) {
            return StringUtils::Trim(line.substr(0, i));
        }
    }
    return StringUtils::Trim(line);
}

std::string decode_value(std::string value) {
    value = StringUtils::Trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }

    std::string decoded;
    decoded.reserve(value.size());
    bool escaped = false;
    for (char c : value) {
        if (!escaped) {
            if (c == '\\') {
                escaped = true;
            } else {
                decoded.push_back(c);
            }
            continue;
        }

        switch (c) {
            case 'n': decoded.push_back('\n'); break;
            case 'r': decoded.push_back('\r'); break;
            case 't': decoded.push_back('\t'); break;
            case '\\': decoded.push_back('\\'); break;
            case '"': decoded.push_back('"'); break;
            default:
                decoded.push_back(c);
                break;
        }
        escaped = false;
    }
    if (escaped) {
        decoded.push_back('\\');
    }
    return decoded;
}

std::string encode_value(const std::string& value) {
    bool needs_quotes = value.empty();
    std::string encoded;
    encoded.reserve(value.size() + 2);
    for (char c : value) {
        switch (c) {
            case '\n': encoded += "\\n"; needs_quotes = true; break;
            case '\r': encoded += "\\r"; needs_quotes = true; break;
            case '\t': encoded += "\\t"; needs_quotes = true; break;
            case '\\': encoded += "\\\\"; needs_quotes = true; break;
            case '"': encoded += "\\\""; needs_quotes = true; break;
            case ';':
            case '#':
                encoded.push_back(c);
                needs_quotes = true;
                break;
            default:
                if (std::isspace(static_cast<unsigned char>(c))) {
                    needs_quotes = true;
                }
                encoded.push_back(c);
                break;
        }
    }
    if (needs_quotes) {
        return "\"" + encoded + "\"";
    }
    return encoded;
}

std::vector<std::string> normalize_lines(const std::string& content) {
    std::vector<std::string> raw_lines = StringUtils::Split(content, '\n');
    std::vector<std::string> lines;
    std::string pending;

    for (std::string line : raw_lines) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string trimmed = StringUtils::Trim(line);
        bool continued = !trimmed.empty() && trimmed.back() == '\\';
        if (continued) {
            trimmed.pop_back();
            pending += StringUtils::Trim(trimmed);
            continue;
        }
        if (!pending.empty()) {
            pending += StringUtils::Trim(trimmed);
            lines.push_back(pending);
            pending.clear();
        } else {
            lines.push_back(line);
        }
    }

    if (!pending.empty()) {
        lines.push_back(pending);
    }
    return lines;
}

} // namespace

bool ConfigParser::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return LoadFromString(buffer.str());
}

bool ConfigParser::LoadFromString(const std::string& content) {
    Clear();
    std::vector<std::string> lines = normalize_lines(content);
    std::string current_section = "";

    for (std::string& line : lines) {
        line = StringUtils::Trim(line);
        if (line.empty()) continue;
        
        // Strip comment
        if (line[0] == ';' || line[0] == '#') continue;

        line = strip_inline_comment(line);
        
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
        val = decode_value(val);

        if (!key.empty()) {
            data_[current_section][key] = val;
        }
    }

    return true;
}

std::string ConfigParser::SaveToString() const {
    std::ostringstream out;
    std::vector<std::string> sections = GetSections();
    std::sort(sections.begin(), sections.end());

    auto write_section = [&](const std::string& section) {
        if (!section.empty()) {
            out << "[" << section << "]\n";
        }
        std::vector<std::string> keys = GetKeys(section);
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            out << key << " = " << encode_value(GetString(section, key)) << "\n";
        }
        out << "\n";
    };

    if (HasSection("")) {
        write_section("");
    }
    for (const auto& section : sections) {
        if (!section.empty()) {
            write_section(section);
        }
    }
    return out.str();
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
