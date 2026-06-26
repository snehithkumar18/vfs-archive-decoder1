#include "path_utils.h"
#include <algorithm>
#include <sstream>

bool VFSPathUtils::is_absolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}

std::string VFSPathUtils::canonicalize(const std::string& path) {
    if (path.empty()) return "/";
    
    std::vector<std::string> parts = split(path);
    std::vector<std::string> resolved;
    
    for (const auto& part : parts) {
        if (part == "." || part.empty()) {
            continue;
        } else if (part == "..") {
            if (!resolved.empty()) {
                resolved.pop_back();
            }
        } else {
            resolved.push_back(part);
        }
    }
    
    if (resolved.empty()) return "/";
    
    std::string result;
    for (const auto& dir : resolved) {
        result += "/" + dir;
    }
    return result;
}

std::vector<std::string> VFSPathUtils::split(const std::string& path) {
    std::vector<std::string> result;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::string VFSPathUtils::join(const std::string& parent, const std::string& child) {
    if (parent.empty()) return canonicalize(child);
    if (child.empty()) return canonicalize(parent);
    
    std::string p = parent;
    if (p.back() == '/') {
        p.pop_back();
    }
    
    std::string c = child;
    if (c.front() == '/') {
        c = c.substr(1);
    }
    
    return canonicalize(p + "/" + c);
}

std::string VFSPathUtils::get_filename(const std::string& path) {
    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos) {
        return path;
    }
    return path.substr(last_slash + 1);
}

std::string VFSPathUtils::get_parent_directory(const std::string& path) {
    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos) {
        return "/";
    }
    if (last_slash == 0) {
        return "/";
    }
    return path.substr(0, last_slash);
}

std::string VFSPathUtils::get_extension(const std::string& path) {
    std::string filename = get_filename(path);
    size_t last_dot = filename.find_last_of('.');
    if (last_dot == std::string::npos || last_dot == 0) {
        return "";
    }
    return filename.substr(last_dot + 1);
}

bool VFSPathUtils::validate_characters(const std::string& path) {
    // Allows alphanumerics, slashes, periods, underscores and dashes
    for (char c : path) {
        if (!std::isalnum(c) && c != '/' && c != '.' && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}
