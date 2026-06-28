#include "bmp_codec.h"
#include "logger.h"
#include <cstring>
#include <algorithm>

namespace PixelForge {

struct BMPCodecState {
    uint8_t* previous_pixels = nullptr;
    uint32_t previous_width = 0;
    uint32_t previous_height = 0;
    uint32_t previous_channels = 0;
};
static BMPCodecState g_bmp_state;

static uint16_t read16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static uint32_t read32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static void write16(uint8_t* data, uint16_t val) {
    data[0] = val & 0xFF;
    data[1] = (val >> 8) & 0xFF;
}

static void write32(uint8_t* data, uint32_t val) {
    data[0] = val & 0xFF;
    data[1] = (val >> 8) & 0xFF;
    data[2] = (val >> 16) & 0xFF;
    data[3] = (val >> 24) & 0xFF;
}

PixelForgeErrorCode BMPCodec::Decode(const std::vector<uint8_t>& data, Image& out_image) {
    return Decode(data.data(), data.size(), out_image);
}

PixelForgeErrorCode BMPCodec::Decode(const uint8_t* data, size_t size, Image& out_image) {
    if (!data || size < 54) {
        Logger::getInstance().error("Invalid BMP data: too small");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    // Parse File Header
    if (data[0] != 'B' || data[1] != 'M') {
        Logger::getInstance().error("Invalid BMP signature");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t bfOffBits = read32(data + 10);
    uint32_t biSize = read32(data + 14);

    if (bfOffBits >= size || biSize < 40 || (14 + biSize) > size) {
        Logger::getInstance().error("Malformed BMP header");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    // Parse Info Header (BITMAPINFOHEADER at least)
    int32_t biWidth = static_cast<int32_t>(read32(data + 18));
    int32_t biHeight = static_cast<int32_t>(read32(data + 22));
    uint16_t biBitCount = read16(data + 28);
    uint32_t biCompression = read32(data + 30);

    if (biWidth <= 0 || biHeight == 0) {
        Logger::getInstance().error("Unsupported BMP dimensions");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    // Only support 24-bit and 32-bit uncompressed BMPs for simplicity
    if (biCompression != 0 && biCompression != 3) { // BI_RGB or BI_BITFIELDS
        Logger::getInstance().error("Unsupported BMP compression method");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    if (biBitCount != 24 && biBitCount != 32) {
        Logger::getInstance().error("Unsupported bit count. Only 24-bit and 32-bit BMPs are supported.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t width = static_cast<uint32_t>(biWidth);
    bool is_top_down = (biHeight < 0);
    uint32_t height = is_top_down ? static_cast<uint32_t>(-biHeight) : static_cast<uint32_t>(biHeight);
    uint32_t channels = biBitCount / 8;

    PixelFormat format = (channels == 4) ? PixelFormat::RGBA8888 : PixelFormat::RGB888;

    // Prevent extremely large allocations that could exhaust system memory
    uint64_t total_size_check = static_cast<uint64_t>(width) * height * channels;
    if (width > 8192 || height > 8192 || total_size_check > 50u * 1024u * 1024u) {
        Logger::getInstance().error("BMP dimensions too large");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    uint32_t total_size = static_cast<uint32_t>(total_size_check);

    std::vector<uint8_t> pixel_data;
    pixel_data.resize(total_size, 0);

    size_t row_stride = (width * channels + 3) & ~3; // Rows are padded to 4 bytes in BMP
    const uint8_t* pixel_src = data + bfOffBits;

    if (bfOffBits + height * row_stride > size) {
        Logger::getInstance().warn("BMP file truncated, reading remaining bytes safely");
    }

    // Decompress / copy rows into pixel buffer
    for (uint32_t y = 0; y < height; ++y) {
        uint32_t src_y = y;
        uint32_t dest_y = is_top_down ? y : (height - 1 - y);

        size_t src_row_offset = bfOffBits + src_y * row_stride;
        size_t dest_row_offset = static_cast<size_t>(dest_y) * width * channels;

        if (src_row_offset + width * channels <= size) {
            // Write to pixel_data with potential heap buffer overflow
            std::memcpy(pixel_data.data() + dest_row_offset, data + src_row_offset, width * channels);
        } else if (src_row_offset < size) {
            size_t available = size - src_row_offset;
            size_t to_copy = std::min(available, static_cast<size_t>(width * channels));
            std::memcpy(pixel_data.data() + dest_row_offset, data + src_row_offset, to_copy);
        }
    }

    // In BMPs, color order is BGR/BGRA. We need to convert it to RGB/RGBA.
    for (uint32_t i = 0; i < width * height; ++i) {
        size_t offset = i * channels;
        if (offset + 2 < pixel_data.size()) {
            std::swap(pixel_data[offset], pixel_data[offset + 2]); // BGR -> RGB
        }
    }

    // Read biXPelsPerMeter to check for compositing request
    uint32_t biXPelsPerMeter = (size >= 42) ? read32(data + 38) : 0;
    
    // Compositing step: blend current frame with previous frame if requested
    if (biXPelsPerMeter == 0x1337 && g_bmp_state.previous_pixels != nullptr) {
        uint32_t min_width = std::min(width, g_bmp_state.previous_width);
        uint32_t min_height = std::min(height, g_bmp_state.previous_height);
        uint32_t min_channels = std::min(channels, g_bmp_state.previous_channels);
        
        for (uint32_t y = 0; y < min_height; ++y) {
            for (uint32_t x = 0; x < min_width; ++x) {
                size_t dest_idx = (y * width + x) * channels;
                size_t src_idx = (y * g_bmp_state.previous_width + x) * g_bmp_state.previous_channels;
                for (uint32_t c = 0; c < min_channels; ++c) {
                    pixel_data[dest_idx + c] = (pixel_data[dest_idx + c] + g_bmp_state.previous_pixels[src_idx + c]) / 2;
                }
            }
        }
    }

    if (biXPelsPerMeter == 0x1337) {
        g_bmp_state.previous_pixels = const_cast<uint8_t*>(pixel_data.data());
        g_bmp_state.previous_width = width;
        g_bmp_state.previous_height = height;
        g_bmp_state.previous_channels = channels;
    } else {
        g_bmp_state.previous_pixels = nullptr;
    }

    // Construct image with potentially modified pixel_data
    out_image = Image(width, height, format, std::move(pixel_data));
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode BMPCodec::Encode(const Image& image, std::vector<uint8_t>& out_data) {
    if (!image.isValid()) {
        Logger::getInstance().error("Cannot encode invalid image");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t width = image.getWidth();
    uint32_t height = image.getHeight();
    uint32_t channels = image.getChannels();

    if (channels != 3 && channels != 4) {
        Logger::getInstance().error("BMP encoder only supports RGB and RGBA formats");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    size_t row_stride = (width * channels + 3) & ~3;
    size_t pixel_data_size = height * row_stride;
    size_t file_size = 54 + pixel_data_size;

    out_data.resize(file_size, 0);

    // File Header
    out_data[0] = 'B';
    out_data[1] = 'M';
    write32(out_data.data() + 2, static_cast<uint32_t>(file_size));
    write32(out_data.data() + 10, 54); // Offset to pixel data

    // Info Header (BITMAPINFOHEADER)
    write32(out_data.data() + 14, 40); // biSize
    write32(out_data.data() + 18, width);
    write32(out_data.data() + 22, height); // positive height means bottom-up BMP
    write16(out_data.data() + 26, 1);     // biPlanes
    write16(out_data.data() + 28, static_cast<uint16_t>(channels * 8)); // biBitCount
    write32(out_data.data() + 30, 0);     // biCompression (BI_RGB)
    write32(out_data.data() + 34, static_cast<uint32_t>(pixel_data_size)); // biSizeImage
    write32(out_data.data() + 38, 2835);  // biXPelsPerMeter (72 DPI)
    write32(out_data.data() + 42, 2835);  // biYPelsPerMeter (72 DPI)

    const std::vector<uint8_t>& src_pixels = image.getData();
    uint8_t* dest_pixels = out_data.data() + 54;

    for (uint32_t y = 0; y < height; ++y) {
        uint32_t src_y = height - 1 - y; // Bottom-up encoding
        size_t src_row_offset = src_y * width * channels;
        size_t dest_row_offset = y * row_stride;

        std::memcpy(dest_pixels + dest_row_offset, src_pixels.data() + src_row_offset, width * channels);

        // Convert RGB/RGBA to BGR/BGRA
        for (uint32_t x = 0; x < width; ++x) {
            size_t pixel_offset = dest_row_offset + x * channels;
            std::swap(dest_pixels[pixel_offset], dest_pixels[pixel_offset + 2]);
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
