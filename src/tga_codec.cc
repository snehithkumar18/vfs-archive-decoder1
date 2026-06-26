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

    if (width == 0 || height == 0) {
        Logger::getInstance().error("Invalid TGA dimensions");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    // Only support 24-bit and 32-bit true color images (type 2 and type 10)
    if (image_type != 2 && image_type != 10) {
        Logger::getInstance().error("Unsupported TGA image type: " + std::to_string(image_type));
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    if (pixel_depth != 24 && pixel_depth != 32) {
        Logger::getInstance().error("Unsupported TGA depth. Only 24-bit and 32-bit are supported.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t channels = pixel_depth / 8;
    PixelFormat format = (channels == 4) ? PixelFormat::RGBA8888 : PixelFormat::RGB888;

    size_t header_offset = 18 + id_length;
    if (color_map_type == 1) {
        uint16_t color_map_len = read16(data + 5);
        uint8_t color_map_entry_size = data[7];
        header_offset += (color_map_len * (color_map_entry_size / 8));
    }

    if (header_offset > size) {
        Logger::getInstance().error("TGA file header offset out of bounds");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    // Allocate memory for output image
    std::vector<uint8_t> pixel_data(width * height * channels, 0);
    size_t dest_size = pixel_data.size();

    size_t src_offset = header_offset;
    size_t dest_offset = 0;

    if (image_type == 2) {
        // Uncompressed true-color
        size_t expected_bytes = width * height * channels;
        if (src_offset + expected_bytes > size) {
            expected_bytes = size - src_offset;
            Logger::getInstance().warn("TGA data is truncated, copying available bytes");
        }
        std::memcpy(pixel_data.data(), data + src_offset, expected_bytes);
    } else {
        // Run-length encoded true-color (type 10)
        while (dest_offset < dest_size) {
            if (src_offset >= size) {
                // Incomplete stream
                break;
            }

            uint8_t packet_header = data[src_offset++];
            uint32_t count = (packet_header & 0x7F) + 1;
            bool is_rle = (packet_header & 0x80) != 0;

            if (dest_offset + count * channels > dest_size) {
                // Clamp count to prevent writing out of bounds on output buffer
                count = (dest_size - dest_offset) / channels;
                if (count == 0) break;
            }

            if (is_rle) {
                // RLE packet: repeat one pixel value
                if (src_offset + channels > size) {
                    Logger::getInstance().error("Invalid RLE packet: unexpected end of stream");
                    break;
                }
                const uint8_t* pixel_to_repeat = data + src_offset;
                src_offset += channels;

                for (uint32_t i = 0; i < count; ++i) {
                    std::memcpy(pixel_data.data() + dest_offset, pixel_to_repeat, channels);
                    dest_offset += channels;
                }
            } else {
                // Raw packet: copy unique pixel values
                // Raw packet: copy unique pixel values directly
                std::memcpy(pixel_data.data() + dest_offset, data + src_offset, count * channels);
                src_offset += count * channels;
                dest_offset += count * channels;
            }
        }
    }

    // Convert BGR/BGRA (TGA default) to RGB/RGBA
    for (uint32_t i = 0; i < width * height; ++i) {
        size_t offset = i * channels;
        if (offset + 2 < pixel_data.size()) {
            std::swap(pixel_data[offset], pixel_data[offset + 2]);
        }
    }

    // Handle vertical flipping if necessary
    // TGA origin is determined by bits 4 and 5 of the image descriptor:
    // bit 4: 0 = left origin, 1 = right origin
    // bit 5: 0 = bottom origin, 1 = top origin
    bool is_top_origin = (image_descriptor & 0x20) != 0;
    if (!is_top_origin) {
        // Flip vertically to match top-down image layout
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
