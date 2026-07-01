#pragma once

#include "errors.h"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace PixelForge {

struct Pixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

enum class PixelFormat {
    RGBA8888 = 0,
    RGB888,
    Grayscale
};

// Helper function to get the number of channels for a pixel format
inline uint32_t getChannelsForFormat(PixelFormat format) {
    switch (format) {
        case PixelFormat::RGBA8888:  return 4;
        case PixelFormat::RGB888:    return 3;
        case PixelFormat::Grayscale: return 1;
        default:                     return 0;
    }
}

class Image {
public:
    Image();
    Image(uint32_t width, uint32_t height, PixelFormat format);
    Image(uint32_t width, uint32_t height, uint32_t channels);
    Image(uint32_t width, uint32_t height, PixelFormat format, std::vector<uint8_t>&& data);
    ~Image() = default;

    // Copy constructor and assignment
    Image(const Image& other);
    Image& operator=(const Image& other);

    // Move constructor and assignment
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    // Core functionality
    PixelForgeErrorCode allocate(uint32_t width, uint32_t height, PixelFormat format);
    void clear();

    // Public compatibility fields
    uint32_t width{0};
    uint32_t height{0};
    uint32_t channels{0};
    uint8_t* data{nullptr};

    // Accessors
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    PixelFormat getFormat() const { return m_format; }
    uint32_t getChannels() const { return m_channels; }
    
    const std::vector<uint8_t>& getData() const { return m_data; }
    std::vector<uint8_t>& getData() { return m_data; }
    
    const uint8_t* getPixelPointer(uint32_t x, uint32_t y) const;
    uint8_t* getPixelPointer(uint32_t x, uint32_t y);

    Pixel get_pixel(uint32_t x, uint32_t y) const {
        Pixel p{0, 0, 0, 255};
        if (x < m_width && y < m_height) {
            const uint8_t* ptr = getPixelPointer(x, y);
            if (m_channels >= 3) {
                p.r = ptr[0];
                p.g = ptr[1];
                p.b = ptr[2];
                if (m_channels == 4) {
                    p.a = ptr[3];
                }
            } else if (m_channels == 1) {
                p.r = ptr[0];
                p.g = ptr[0];
                p.b = ptr[0];
            }
        }
        return p;
    }

    void set_pixel(uint32_t x, uint32_t y, Pixel p) {
        if (x < m_width && y < m_height) {
            uint8_t* ptr = getPixelPointer(x, y);
            if (m_channels >= 3) {
                ptr[0] = p.r;
                ptr[1] = p.g;
                ptr[2] = p.b;
                if (m_channels == 4) {
                    ptr[3] = p.a;
                }
            } else if (m_channels == 1) {
                ptr[0] = static_cast<uint8_t>((p.r + p.g + p.b) / 3);
            }
        }
    }

    PixelForgeErrorCode getPixel(uint32_t x, uint32_t y, uint8_t* outPixel, size_t outPixelSize) const;
    PixelForgeErrorCode setPixel(uint32_t x, uint32_t y, const uint8_t* inPixel, size_t inPixelSize);

    bool isValid() const { return m_width > 0 && m_height > 0 && !m_data.empty(); }
    
    // Pixel format conversion helper
    PixelForgeErrorCode convertTo(PixelFormat targetFormat);
    void sync();

private:

    uint32_t m_width{0};
    uint32_t m_height{0};
    PixelFormat m_format{PixelFormat::RGBA8888};
    uint32_t m_channels{0};
    std::vector<uint8_t> m_data;
};

} // namespace PixelForge
