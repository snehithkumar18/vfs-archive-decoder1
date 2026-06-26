#include "string_utils.h"
#include <cstring>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace PixelForge {
namespace StringUtils {

static const char* kBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline bool is_base64(uint8_t c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string Base64Encode(const uint8_t* data, size_t length) {
    std::string ret;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += kBase64Chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; (j < i + 1); j++)
            ret += kBase64Chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

std::string Base64Encode(const std::vector<uint8_t>& data) {
    return Base64Encode(data.data(), data.size());
}

std::string Base64Encode(const std::string& data) {
    return Base64Encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool Base64Decode(const std::string& base64_str, std::vector<uint8_t>& out_data) {
    int in_len = base64_str.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    out_data.clear();

    while (in_len-- && (base64_str[in_] != '=') && is_base64(base64_str[in_])) {
        char_array_4[i++] = base64_str[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                const char* p = strchr(kBase64Chars, char_array_4[i]);
                char_array_4[i] = p ? (p - kBase64Chars) : 0;
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                out_data.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++) {
            const char* p = strchr(kBase64Chars, char_array_4[j]);
            char_array_4[j] = p ? (p - kBase64Chars) : 0;
        }

        for (j = i; j < 4; j++)
            char_array_4[j] = 0;

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; (j < i - 1); j++) out_data.push_back(char_array_3[j]);
    }

    return true;
}

std::vector<uint8_t> Base64Decode(const std::string& base64_str) {
    std::vector<uint8_t> out;
    Base64Decode(base64_str, out);
    return out;
}

std::string HexEncode(const uint8_t* data, size_t length, bool uppercase) {
    std::string ret;
    const char* hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        ret += hex_chars[data[i] >> 4];
        ret += hex_chars[data[i] & 0x0F];
    }
    return ret;
}

std::string HexEncode(const std::vector<uint8_t>& data, bool uppercase) {
    return HexEncode(data.data(), data.size(), uppercase);
}

bool HexDecode(const std::string& hex_str, std::vector<uint8_t>& out_data) {
    out_data.clear();
    size_t start = 0;
    if (hex_str.size() >= 2 && hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        start = 2;
    }
    
    std::string clean_str;
    for (size_t i = start; i < hex_str.size(); ++i) {
        if (!isspace(hex_str[i])) {
            clean_str.push_back(hex_str[i]);
        }
    }

    if (clean_str.size() % 2 != 0) return false;

    auto hex_to_val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    for (size_t i = 0; i < clean_str.size(); i += 2) {
        int high = hex_to_val(clean_str[i]);
        int low = hex_to_val(clean_str[i + 1]);
        if (high == -1 || low == -1) return false;
        out_data.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return true;
}

std::vector<uint8_t> HexDecode(const std::string& hex_str) {
    std::vector<uint8_t> out;
    HexDecode(hex_str, out);
    return out;
}

static bool GlobMatchHelper(const char* pat, const char* str) {
    if (*pat == '\0') return *str == '\0';
    if (*pat == '*') {
        while (*pat == '*') pat++;
        if (*pat == '\0') return true;
        while (*str != '\0') {
            if (GlobMatchHelper(pat, str)) return true;
            str++;
        }
        return false;
    }
    if (*pat == '?' && *str != '\0') {
        return GlobMatchHelper(pat + 1, str + 1);
    }
    if (*pat == *str) {
        return GlobMatchHelper(pat + 1, str + 1);
    }
    return false;
}

bool GlobMatch(const std::string& pattern, const std::string& str) {
    return GlobMatchHelper(pattern.c_str(), str.c_str());
}

std::vector<std::string> Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> Split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do {
        pos = str.find(delimiter, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos - prev);
        tokens.push_back(token);
        prev = pos + delimiter.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

std::string Join(const std::vector<std::string>& elements, const std::string& delimiter) {
    std::string ret;
    for (size_t i = 0; i < elements.size(); ++i) {
        ret += elements[i];
        if (i + 1 < elements.size()) {
            ret += delimiter;
        }
    }
    return ret;
}

std::string TrimLeft(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && isspace(str[start])) {
        start++;
    }
    return str.substr(start);
}

std::string TrimRight(const std::string& str) {
    size_t end = str.size();
    while (end > 0 && isspace(str[end - 1])) {
        end--;
    }
    return str.substr(0, end);
}

std::string Trim(const std::string& str) {
    return TrimRight(TrimLeft(str));
}

} // namespace StringUtils
} // namespace PixelForge
