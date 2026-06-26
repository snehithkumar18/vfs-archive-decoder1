#ifndef PIXELFORGE_STRING_UTILS_H
#define PIXELFORGE_STRING_UTILS_H

#include <string>
#include <vector>
#include <cstdint>

namespace PixelForge {
namespace StringUtils {

// Base64 encoding/decoding
std::string Base64Encode(const uint8_t* data, size_t length);
std::string Base64Encode(const std::vector<uint8_t>& data);
std::string Base64Encode(const std::string& data);

bool Base64Decode(const std::string& base64_str, std::vector<uint8_t>& out_data);
std::vector<uint8_t> Base64Decode(const std::string& base64_str);

// Hex encoding/decoding
std::string HexEncode(const uint8_t* data, size_t length, bool uppercase = false);
std::string HexEncode(const std::vector<uint8_t>& data, bool uppercase = false);

bool HexDecode(const std::string& hex_str, std::vector<uint8_t>& out_data);
std::vector<uint8_t> HexDecode(const std::string& hex_str);

// Glob matching with wildcard '*' and '?'
// '*' matches 0 or more characters.
// '?' matches exactly 1 character.
bool GlobMatch(const std::string& pattern, const std::string& str);

// String split/join/trim
std::vector<std::string> Split(const std::string& str, char delimiter);
std::vector<std::string> Split(const std::string& str, const std::string& delimiter);

std::string Join(const std::vector<std::string>& elements, const std::string& delimiter);

std::string TrimLeft(const std::string& str);
std::string TrimRight(const std::string& str);
std::string Trim(const std::string& str);

} // namespace StringUtils
} // namespace PixelForge

#endif // PIXELFORGE_STRING_UTILS_H
