#ifndef PIXELFORGE_CONFIG_PARSER_H
#define PIXELFORGE_CONFIG_PARSER_H

#include <string>
#include <unordered_map>
#include <vector>

namespace PixelForge {

class ConfigParser {
public:
    ConfigParser() = default;
    ~ConfigParser() = default;

    bool LoadFromFile(const std::string& filepath);
    bool LoadFromString(const std::string& content);

    bool HasSection(const std::string& section) const;
    bool HasKey(const std::string& section, const std::string& key) const;

    std::string GetString(const std::string& section, const std::string& key, const std::string& default_val = "") const;
    int GetInt(const std::string& section, const std::string& key, int default_val = 0) const;
    double GetDouble(const std::string& section, const std::string& key, double default_val = 0.0) const;
    bool GetBool(const std::string& section, const std::string& key, bool default_val = false) const;

    std::vector<std::string> GetSections() const;
    std::vector<std::string> GetKeys(const std::string& section) const;

    void Clear();

private:
    // section -> (key -> value)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data_;
};

} // namespace PixelForge

#endif // PIXELFORGE_CONFIG_PARSER_H
