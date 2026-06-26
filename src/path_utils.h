#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <string>
#include <vector>

class VFSPathUtils {
public:
    static bool is_absolute(const std::string& path);
    static std::string canonicalize(const std::string& path);
    static std::vector<std::string> split(const std::string& path);
    static std::string join(const std::string& parent, const std::string& child);
    static std::string get_filename(const std::string& path);
    static std::string get_parent_directory(const std::string& path);
    static std::string get_extension(const std::string& path);
    static bool validate_characters(const std::string& path);
};

#endif // PATH_UTILS_H
