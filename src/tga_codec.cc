#include "tga_codec.h"
#include "logger.h"
#include <cstring>
#include <algorithm>

namespace PixelForge {

static uint16_t read16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static void write16(uint8_t* data, uint16_t val) {
    data[0] = val & 0xFF;
    data[1] = (val >> 8) & 0xFF;
}

PixelForgeErrorCode TGACodec::Decode(const std::vector<uint8_t>& data, Image& out_image) {
    return Decode(data.data(), data.size(), out_image);
}

PixelForgeErrorCode TGACodec::Decode(const uint8_t* data, size_t size, Image& out_image) {
    if (!data || size < 18) {
        Logger::getInstance().error("Invalid TGA data: too small");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint8_t id_length = data[0];
    uint8_t color_map_type = data[1];
    uint8_t image_type = data[2];
    
    uint16_t width = read16(data + 12);
    uint16_t height = read16(data + 14);
    uint8_t pixel_depth = data[16];
    uint8_t image_descriptor = data[17];

    // Strict bounds checks to prevent OOM
    if (width == 0 || height == 0 || width > 1024 || height > 1024) {
        Logger::getInstance().error("Invalid or too large TGA dimensions");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    bool is_color_mapped = (image_type == 1 || image_type == 9);
    bool is_true_color = (image_type == 2 || image_type == 10);

    if (!is_color_mapped && !is_true_color) {
        Logger::getInstance().error("Unsupported TGA image type: " + std::to_string(image_type));
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    if (is_color_mapped) {
        if (pixel_depth != 8) {
            Logger::getInstance().error("Color-mapped TGA must have 8-bit depth");
            return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
        }
    } else {
        if (pixel_depth != 24 && pixel_depth != 32) {
            Logger::getInstance().error("Unsupported TGA depth. Only 24-bit and 32-bit are supported.");
            return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
        }
    }

    size_t header_offset = 18 + id_length;
    uint16_t color_map_len = read16(data + 5);
    uint8_t color_map_entry_size = data[7];
    if (color_map_type == 1) {
        header_offset += (color_map_len * (color_map_entry_size / 8));
    }

    if (header_offset > size) {
        Logger::getInstance().error("TGA file header offset out of bounds");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t palette_channels = color_map_entry_size / 8;
    if (is_color_mapped && color_map_type != 1) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (is_color_mapped && palette_channels != 3 && palette_channels != 4) {
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }
    size_t map_bytes = static_cast<size_t>(color_map_len) * palette_channels;
    std::vector<uint8_t> color_map;
    if (is_color_mapped) {
        size_t map_offset = 18u + id_length;
        if (map_offset > size || map_bytes > size - map_offset) {
            return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
        }
        color_map.assign(data + map_offset, data + map_offset + map_bytes);
    }

    uint32_t channels = is_color_mapped ? palette_channels : (pixel_depth / 8);
    if (channels != 3 && channels != 4) {
        channels = 3; // fallback
    }
    PixelFormat format = (channels == 4) ? PixelFormat::RGBA8888 : PixelFormat::RGB888;

    // Decode pixel indices (if color-mapped) or raw pixels
    uint32_t decode_channels = is_color_mapped ? 1 : channels;
    std::vector<uint8_t> decoded_buffer(width * height * decode_channels, 0);
    size_t dest_size = decoded_buffer.size();

    size_t src_offset = header_offset;
    size_t dest_offset = 0;

    if (image_type == 1 || image_type == 2) {
        // Uncompressed
        size_t expected_bytes = width * height * decode_channels;
        if (src_offset + expected_bytes > size) {
            expected_bytes = size - src_offset;
        }
        std::memcpy(decoded_buffer.data(), data + src_offset, expected_bytes);
    } else {
        // RLE
        while (dest_offset < dest_size) {
            if (src_offset >= size) break;

            uint8_t packet_header = data[src_offset++];
            uint32_t count = (packet_header & 0x7F) + 1;
            bool is_rle = (packet_header & 0x80) != 0;

            if (dest_offset + count * decode_channels > dest_size) {
                count = (dest_size - dest_offset) / decode_channels;
                if (count == 0) break;
            }

            if (is_rle) {
                if (src_offset + decode_channels > size) break;
                const uint8_t* pixel_to_repeat = data + src_offset;
                src_offset += decode_channels;

                for (uint32_t i = 0; i < count; ++i) {
                    std::memcpy(decoded_buffer.data() + dest_offset, pixel_to_repeat, decode_channels);
                    dest_offset += decode_channels;
                }
            } else {
                if (src_offset + count * decode_channels > size) {
                    count = (size - src_offset) / decode_channels;
                }
                std::memcpy(decoded_buffer.data() + dest_offset, data + src_offset, count * decode_channels);
                src_offset += count * decode_channels;
                dest_offset += count * decode_channels;
            }
        }
    }

    // Resolve indices through the palette owned by this decode operation.
    std::vector<uint8_t> pixel_data(width * height * channels, 0);
    if (is_color_mapped) {
        for (uint32_t i = 0; i < width * height; ++i) {
            uint8_t idx = decoded_buffer[i];
            if (idx < color_map_len) {
                size_t map_offset = static_cast<size_t>(idx) * palette_channels;
                std::memcpy(pixel_data.data() + static_cast<size_t>(i) * channels,
                            color_map.data() + map_offset,
                            channels);
            }
        }
    } else {
        pixel_data = std::move(decoded_buffer);
    }

    // Convert BGR/BGRA to RGB/RGBA
    for (uint32_t i = 0; i < width * height; ++i) {
        size_t offset = i * channels;
        if (offset + 2 < pixel_data.size()) {
            std::swap(pixel_data[offset], pixel_data[offset + 2]);
        }
    }

    // Handle vertical flipping if necessary
    bool is_top_origin = (image_descriptor & 0x20) != 0;
    if (!is_top_origin) {
        std::vector<uint8_t> flipped(pixel_data.size());
        size_t row_size = width * channels;
        for (uint32_t y = 0; y < height; ++y) {
            uint32_t src_y = height - 1 - y;
            std::memcpy(flipped.data() + y * row_size, pixel_data.data() + src_y * row_size, row_size);
        }
        pixel_data = std::move(flipped);
    }

    out_image = Image(width, height, format, std::move(pixel_data));
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode TGACodec::Encode(const Image& image, std::vector<uint8_t>& out_data, bool compress) {
    if (!image.isValid()) {
        Logger::getInstance().error("Cannot encode invalid image");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t width = image.getWidth();
    uint32_t height = image.getHeight();
    uint32_t channels = image.getChannels();

    if (channels != 3 && channels != 4) {
        Logger::getInstance().error("TGA encoder only supports RGB and RGBA formats");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    // Write header (18 bytes)
    out_data.resize(18);
    out_data[0] = 0; // id_length
    out_data[1] = 0; // color_map_type (no color map)
    out_data[2] = compress ? 10 : 2; // image_type (2=uncompressed true color, 10=RLE true color)
    
    // Color map spec (5 bytes, all 0)
    std::memset(out_data.data() + 3, 0, 5);

    // Image spec (10 bytes)
    write16(out_data.data() + 8, 0);  // x_origin
    write16(out_data.data() + 10, 0); // y_origin
    write16(out_data.data() + 12, static_cast<uint16_t>(width));
    write16(out_data.data() + 14, static_cast<uint16_t>(height));
    out_data[16] = static_cast<uint8_t>(channels * 8); // pixel_depth
    out_data[17] = 0x20; // image_descriptor: origin is top-left

    // Convert source RGB/RGBA to BGR/BGRA for TGA compatibility
    std::vector<uint8_t> bgr_pixels = image.getData();
    for (uint32_t i = 0; i < width * height; ++i) {
        size_t offset = i * channels;
        if (offset + 2 < bgr_pixels.size()) {
            std::swap(bgr_pixels[offset], bgr_pixels[offset + 2]);
        }
    }

    if (!compress) {
        // Simply append raw pixel data
        out_data.insert(out_data.end(), bgr_pixels.begin(), bgr_pixels.end());
    } else {
        // Compress using RLE
        size_t pixel_count = width * height;
        size_t index = 0;

        while (index < pixel_count) {
            // Determine run length
            size_t run_len = 1;
            const uint8_t* current_pixel = bgr_pixels.data() + index * channels;

            while (index + run_len < pixel_count && run_len < 128) {
                const uint8_t* next_pixel = bgr_pixels.data() + (index + run_len) * channels;
                if (std::memcmp(current_pixel, next_pixel, channels) == 0) {
                    run_len++;
                } else {
                    break;
                }
            }

            if (run_len >= 2) {
                // Write RLE packet
                out_data.push_back(static_cast<uint8_t>(0x80 | (run_len - 1)));
                out_data.insert(out_data.end(), current_pixel, current_pixel + channels);
                index += run_len;
            } else {
                // Determine raw packet length
                size_t raw_len = 1;
                while (index + raw_len < pixel_count && raw_len < 128) {
                    const uint8_t* next_pixel = bgr_pixels.data() + (index + raw_len) * channels;
                    
                    // Check if a run of repeats starts here
                    if (index + raw_len + 1 < pixel_count) {
                        const uint8_t* following_pixel = bgr_pixels.data() + (index + raw_len + 1) * channels;
                        if (std::memcmp(next_pixel, following_pixel, channels) == 0) {
                            break; // Stop raw packet to start RLE packet
                        }
                    }
                    raw_len++;
                }

                // Write raw packet
                out_data.push_back(static_cast<uint8_t>(raw_len - 1));
                const uint8_t* raw_src = bgr_pixels.data() + index * channels;
                out_data.insert(out_data.end(), raw_src, raw_src + raw_len * channels);
                index += raw_len;
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
