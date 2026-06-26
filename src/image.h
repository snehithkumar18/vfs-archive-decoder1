#pragma once

#include "errors.h"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace PixelForge {

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
    Image(const Image&) = default;
    Image& operator=(const Image&) = default;

    // Move constructor and assignment
    Image(Image&&) noexcept = default;
    Image& operator=(Image&&) noexcept = default;

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

    PixelForgeErrorCode getPixel(uint32_t x, uint32_t y, uint8_t* outPixel, size_t outPixelSize) const;
    PixelForgeErrorCode setPixel(uint32_t x, uint32_t y, const uint8_t* inPixel, size_t inPixelSize);

    bool isValid() const { return m_width > 0 && m_height > 0 && !m_data.empty(); }
    
    // Pixel format conversion helper
    PixelForgeErrorCode convertTo(PixelFormat targetFormat);

private:
    void sync();

    uint32_t m_width{0};
    uint32_t m_height{0};
    PixelFormat m_format{PixelFormat::RGBA8888};
    uint32_t m_channels{0};
    std::vector<uint8_t> m_data;
};

} // namespace PixelForge
