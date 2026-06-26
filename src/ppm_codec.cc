#include "ppm_codec.h"
#include "logger.h"
#include <sstream>
#include <cstring>
#include <algorithm>

namespace PixelForge {

static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void skip_spaces_and_comments(const uint8_t* data, size_t size, size_t& offset) {
    while (offset < size) {
        char c = static_cast<char>(data[offset]);
        if (is_ws(c)) {
            offset++;
        } else if (c == '#') {
            offset++;
            while (offset < size && data[offset] != '\n' && data[offset] != '\r') {
                offset++;
            }
        } else {
            break;
        }
    }
}

static std::string read_token(const uint8_t* data, size_t size, size_t& offset) {
    skip_spaces_and_comments(data, size, offset);
    std::string token;
    while (offset < size) {
        char c = static_cast<char>(data[offset]);
        if (is_ws(c) || c == '#') {
            break;
        }
        token.push_back(c);
        offset++;
    }
    return token;
}

PixelForgeErrorCode PPMCodec::Decode(const std::vector<uint8_t>& data, Image& out_image) {
    return Decode(data.data(), data.size(), out_image);
}

PixelForgeErrorCode PPMCodec::Decode(const uint8_t* data, size_t size, Image& out_image) {
    if (!data || size < 4) {
        Logger::getInstance().error("PPM data too small");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    size_t offset = 0;
    std::string type = read_token(data, size, offset);
    if (type.size() < 2 || type[0] != 'P') {
        Logger::getInstance().error("Invalid Netpbm signature");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    int format_type = type[1] - '0';
    if (format_type < 1 || format_type > 6) {
        Logger::getInstance().error("Unsupported Netpbm type: P" + std::to_string(format_type));
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    std::string w_str = read_token(data, size, offset);
    std::string h_str = read_token(data, size, offset);
    if (w_str.empty() || h_str.empty()) {
        Logger::getInstance().error("PPM dimensions missing");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    int w = std::stoi(w_str);
    int h = std::stoi(h_str);
    if (w <= 0 || h <= 0) {
        Logger::getInstance().error("Invalid Netpbm dimensions");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    int max_val = 255;
    // PBM formats (P1, P4) don't have max value
    if (format_type != 1 && format_type != 4) {
        std::string max_str = read_token(data, size, offset);
        if (max_str.empty()) {
            Logger::getInstance().error("PPM max value missing");
            return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
        }
        max_val = std::stoi(max_str);
        if (max_val <= 0 || max_val > 65535) {
            Logger::getInstance().error("Unsupported max value: " + std::to_string(max_val));
            return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
        }
    }

    // Single white space character separating header and pixel data
    if (offset < size && is_ws(static_cast<char>(data[offset]))) {
        offset++;
    }

    PixelFormat out_format;
    uint32_t out_channels;
    if (format_type == 1 || format_type == 4 || format_type == 2 || format_type == 5) {
        out_format = PixelFormat::Grayscale;
        out_channels = 1;
    } else {
        out_format = PixelFormat::RGB888;
        out_channels = 3;
    }

    std::vector<uint8_t> pixel_data(static_cast<size_t>(w) * h * out_channels, 0);
    size_t expected_pixels = static_cast<size_t>(w) * h;

    if (format_type == 3) {
        // P3: ASCII RGB
        for (size_t i = 0; i < expected_pixels; ++i) {
            std::string r_s = read_token(data, size, offset);
            std::string g_s = read_token(data, size, offset);
            std::string b_s = read_token(data, size, offset);
            if (r_s.empty() || g_s.empty() || b_s.empty()) break;
            
            pixel_data[i * 3 + 0] = static_cast<uint8_t>((std::stoi(r_s) * 255) / max_val);
            pixel_data[i * 3 + 1] = static_cast<uint8_t>((std::stoi(g_s) * 255) / max_val);
            pixel_data[i * 3 + 2] = static_cast<uint8_t>((std::stoi(b_s) * 255) / max_val);
        }
    }
    else if (format_type == 6) {
        // P6: Binary RGB
        size_t expected_bytes = expected_pixels * 3;
        if (max_val > 255) expected_bytes *= 2; // 16-bit values

        if (offset + expected_bytes > size) {
            Logger::getInstance().warn("P6 file ended prematurely, reading partial data");
            expected_bytes = size - offset;
        }

        if (max_val <= 255) {
            std::memcpy(pixel_data.data(), data + offset, expected_bytes);
        } else {
            // Downscale 16-bit to 8-bit
            for (size_t i = 0; i < expected_pixels; ++i) {
                if (offset + i * 6 + 5 >= size) break;
                uint16_t r = (data[offset + i * 6] << 8) | data[offset + i * 6 + 1];
                uint16_t g = (data[offset + i * 6 + 2] << 8) | data[offset + i * 6 + 3];
                uint16_t b = (data[offset + i * 6 + 4] << 8) | data[offset + i * 6 + 5];
                pixel_data[i * 3 + 0] = static_cast<uint8_t>((r * 255) / max_val);
                pixel_data[i * 3 + 1] = static_cast<uint8_t>((g * 255) / max_val);
                pixel_data[i * 3 + 2] = static_cast<uint8_t>((b * 255) / max_val);
            }
        }
    }
    else if (format_type == 5) {
        // P5: Binary Grayscale
        size_t expected_bytes = expected_pixels;
        if (max_val > 255) expected_bytes *= 2;

        if (offset + expected_bytes > size) {
            expected_bytes = size - offset;
        }

        if (max_val <= 255) {
            std::memcpy(pixel_data.data(), data + offset, expected_bytes);
        } else {
            for (size_t i = 0; i < expected_pixels; ++i) {
                if (offset + i * 2 + 1 >= size) break;
                uint16_t g = (data[offset + i * 2] << 8) | data[offset + i * 2 + 1];
                pixel_data[i] = static_cast<uint8_t>((g * 255) / max_val);
            }
        }
    }
    else if (format_type == 2) {
        // P2: ASCII Grayscale
        for (size_t i = 0; i < expected_pixels; ++i) {
            std::string val_s = read_token(data, size, offset);
            if (val_s.empty()) break;
            pixel_data[i] = static_cast<uint8_t>((std::stoi(val_s) * 255) / max_val);
        }
    }
    else if (format_type == 1) {
        // P1: ASCII PBM
        for (size_t i = 0; i < expected_pixels; ++i) {
            std::string val_s = read_token(data, size, offset);
            if (val_s.empty()) break;
            pixel_data[i] = (std::stoi(val_s) != 0) ? 0 : 255; // 1 means black, 0 means white in PBM
        }
    }
    else if (format_type == 4) {
        // P4: Binary PBM
        size_t byte_idx = 0;
        for (size_t i = 0; i < expected_pixels; ++i) {
            size_t bit_offset = i % 8;
            size_t src_byte_offset = offset + (i / 8);
            if (src_byte_offset >= size) break;
            
            uint8_t byte_val = data[src_byte_offset];
            bool bit = (byte_val & (1 << (7 - bit_offset))) != 0;
            pixel_data[i] = bit ? 0 : 255;
        }
    }

    out_image = Image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), out_format, std::move(pixel_data));
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode PPMCodec::EncodeP6(const Image& image, std::vector<uint8_t>& out_data) {
    if (!image.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    
    uint32_t w = image.getWidth();
    uint32_t h = image.getHeight();
    uint32_t ch = image.getChannels();
    const auto& data = image.getData();

    std::string header = "P6\n" + std::to_string(w) + " " + std::to_string(h) + "\n255\n";
    out_data.resize(header.size());
    std::memcpy(out_data.data(), header.data(), header.size());

    size_t offset = out_data.size();
    out_data.resize(offset + static_cast<size_t>(w) * h * 3);

    for (uint32_t i = 0; i < w * h; ++i) {
        size_t src_idx = i * ch;
        size_t dst_idx = offset + i * 3;
        if (ch >= 3) {
            out_data[dst_idx + 0] = data[src_idx + 0];
            out_data[dst_idx + 1] = data[src_idx + 1];
            out_data[dst_idx + 2] = data[src_idx + 2];
        } else {
            // Expand grayscale to RGB
            out_data[dst_idx + 0] = data[src_idx];
            out_data[dst_idx + 1] = data[src_idx];
            out_data[dst_idx + 2] = data[src_idx];
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode PPMCodec::EncodeP5(const Image& image, std::vector<uint8_t>& out_data) {
    if (!image.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = image.getWidth();
    uint32_t h = image.getHeight();
    uint32_t ch = image.getChannels();
    const auto& data = image.getData();

    std::string header = "P5\n" + std::to_string(w) + " " + std::to_string(h) + "\n255\n";
    out_data.resize(header.size());
    std::memcpy(out_data.data(), header.data(), header.size());

    size_t offset = out_data.size();
    out_data.resize(offset + static_cast<size_t>(w) * h);

    for (uint32_t i = 0; i < w * h; ++i) {
        size_t src_idx = i * ch;
        size_t dst_idx = offset + i;
        if (ch >= 3) {
            // RGB -> Grayscale
            out_data[dst_idx] = static_cast<uint8_t>(0.299f * data[src_idx + 0] + 0.587f * data[src_idx + 1] + 0.114f * data[src_idx + 2]);
        } else {
            out_data[dst_idx] = data[src_idx];
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
