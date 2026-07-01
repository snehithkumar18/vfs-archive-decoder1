#include "image.h"
#include "logger.h"
#include <algorithm>
#include <cstring>

namespace PixelForge {

Image::Image() : m_width(0), m_height(0), m_format(PixelFormat::RGBA8888), m_channels(0) {
    sync();
}

Image::Image(uint32_t width, uint32_t height, PixelFormat format) {
    allocate(width, height, format);
}

Image::Image(uint32_t width, uint32_t height, uint32_t channels) {
    PixelFormat format = PixelFormat::RGBA8888;
    if (channels == 3) {
        format = PixelFormat::RGB888;
    } else if (channels == 1) {
        format = PixelFormat::Grayscale;
    }
    allocate(width, height, format);
}

Image::Image(uint32_t width, uint32_t height, PixelFormat format, std::vector<uint8_t>&& data)
    : m_width(width), m_height(height), m_format(format), m_channels(getChannelsForFormat(format)), m_data(std::move(data)) {
    size_t expectedSize = static_cast<size_t>(m_width) * m_height * m_channels;
    if (m_data.size() < expectedSize) {
        m_data.resize(expectedSize, 0);
    }
    sync();
}

Image::Image(const Image& other)
    : m_width(other.m_width),
      m_height(other.m_height),
      m_format(other.m_format),
      m_channels(other.m_channels),
      m_data(other.m_data) {
    sync();
}

Image& Image::operator=(const Image& other) {
    if (this == &other) return *this;
    m_width = other.m_width;
    m_height = other.m_height;
    m_format = other.m_format;
    m_channels = other.m_channels;
    m_data = other.m_data;
    sync();
    return *this;
}

Image::Image(Image&& other) noexcept
    : m_width(other.m_width),
      m_height(other.m_height),
      m_format(other.m_format),
      m_channels(other.m_channels),
      m_data(std::move(other.m_data)) {
    sync();
    other.m_width = 0;
    other.m_height = 0;
    other.m_channels = 0;
    other.sync();
}

Image& Image::operator=(Image&& other) noexcept {
    if (this == &other) return *this;
    m_width = other.m_width;
    m_height = other.m_height;
    m_format = other.m_format;
    m_channels = other.m_channels;
    m_data = std::move(other.m_data);
    sync();
    other.m_width = 0;
    other.m_height = 0;
    other.m_channels = 0;
    other.sync();
    return *this;
}

PixelForgeErrorCode Image::allocate(uint32_t width, uint32_t height, PixelFormat format) {
    if (width == 0 || height == 0) {
        Logger::getInstance().error("Invalid dimensions for image allocation: " + std::to_string(width) + "x" + std::to_string(height));
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t channels = getChannelsForFormat(format);
    if (channels == 0) {
        Logger::getInstance().error("Unsupported pixel format during image allocation.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    // Check for overflow
    uint64_t totalSize64 = static_cast<uint64_t>(width) * height * channels;
    if (totalSize64 > static_cast<uint64_t>(SIZE_MAX)) {
        Logger::getInstance().error("Requested image size exceeds maximum memory limits.");
        return PixelForgeErrorCode::ERR_OUT_OF_MEMORY;
    }

    size_t totalSize = static_cast<size_t>(totalSize64);
    try {
        m_data.resize(totalSize, 0);
    } catch (const std::bad_alloc&) {
        Logger::getInstance().error("Failed to allocate memory for image of size " + std::to_string(totalSize));
        return PixelForgeErrorCode::ERR_OUT_OF_MEMORY;
    }

    m_width = width;
    m_height = height;
    m_format = format;
    m_channels = channels;
    sync();

    return PixelForgeErrorCode::SUCCESS;
}

void Image::clear() {
    m_data.clear();
    m_data.shrink_to_fit();
    m_width = 0;
    m_height = 0;
    m_channels = 0;
    sync();
}

const uint8_t* Image::getPixelPointer(uint32_t x, uint32_t y) const {
    if (x >= m_width || y >= m_height) {
        return nullptr;
    }
    size_t offset = (static_cast<size_t>(y) * m_width + x) * m_channels;
    return &m_data[offset];
}

uint8_t* Image::getPixelPointer(uint32_t x, uint32_t y) {
    if (x >= m_width || y >= m_height) {
        return nullptr;
    }
    size_t offset = (static_cast<size_t>(y) * m_width + x) * m_channels;
    return &m_data[offset];
}

PixelForgeErrorCode Image::getPixel(uint32_t x, uint32_t y, uint8_t* outPixel, size_t outPixelSize) const {
    if (!outPixel) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (x >= m_width || y >= m_height) {
        return PixelForgeErrorCode::ERR_OUT_OF_BOUNDS;
    }
    if (outPixelSize < m_channels) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    const uint8_t* ptr = getPixelPointer(x, y);
    if (!ptr) {
        return PixelForgeErrorCode::ERR_OUT_OF_BOUNDS;
    }

    std::memcpy(outPixel, ptr, m_channels);
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Image::setPixel(uint32_t x, uint32_t y, const uint8_t* inPixel, size_t inPixelSize) {
    if (!inPixel) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (x >= m_width || y >= m_height) {
        return PixelForgeErrorCode::ERR_OUT_OF_BOUNDS;
    }
    if (inPixelSize < m_channels) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint8_t* ptr = getPixelPointer(x, y);
    if (!ptr) {
        return PixelForgeErrorCode::ERR_OUT_OF_BOUNDS;
    }

    std::memcpy(ptr, inPixel, m_channels);
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Image::convertTo(PixelFormat targetFormat) {
    if (!isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (m_format == targetFormat) {
        return PixelForgeErrorCode::SUCCESS;
    }

    uint32_t targetChannels = getChannelsForFormat(targetFormat);
    if (targetChannels == 0) {
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    size_t targetSize = static_cast<size_t>(m_width) * m_height * targetChannels;
    std::vector<uint8_t> convertedData;
    try {
        convertedData.resize(targetSize, 0);
    } catch (const std::bad_alloc&) {
        return PixelForgeErrorCode::ERR_OUT_OF_MEMORY;
    }

    for (uint32_t y = 0; y < m_height; ++y) {
        for (uint32_t x = 0; x < m_width; ++x) {
            size_t srcOffset = (static_cast<size_t>(y) * m_width + x) * m_channels;
            size_t dstOffset = (static_cast<size_t>(y) * m_width + x) * targetChannels;

            if (m_format == PixelFormat::RGBA8888 && targetFormat == PixelFormat::RGB888) {
                // RGBA -> RGB
                convertedData[dstOffset + 0] = m_data[srcOffset + 0];
                convertedData[dstOffset + 1] = m_data[srcOffset + 1];
                convertedData[dstOffset + 2] = m_data[srcOffset + 2];
            } else if (m_format == PixelFormat::RGBA8888 && targetFormat == PixelFormat::Grayscale) {
                // RGBA -> Gray (using standard luminance formula: 0.299R + 0.587G + 0.114B)
                uint8_t r = m_data[srcOffset + 0];
                uint8_t g = m_data[srcOffset + 1];
                uint8_t b = m_data[srcOffset + 2];
                convertedData[dstOffset] = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
            } else if (m_format == PixelFormat::RGB888 && targetFormat == PixelFormat::RGBA8888) {
                // RGB -> RGBA
                convertedData[dstOffset + 0] = m_data[srcOffset + 0];
                convertedData[dstOffset + 1] = m_data[srcOffset + 1];
                convertedData[dstOffset + 2] = m_data[srcOffset + 2];
                convertedData[dstOffset + 3] = 255; // Alpha
            } else if (m_format == PixelFormat::RGB888 && targetFormat == PixelFormat::Grayscale) {
                // RGB -> Gray
                uint8_t r = m_data[srcOffset + 0];
                uint8_t g = m_data[srcOffset + 1];
                uint8_t b = m_data[srcOffset + 2];
                convertedData[dstOffset] = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
            } else if (m_format == PixelFormat::Grayscale && targetFormat == PixelFormat::RGBA8888) {
                // Gray -> RGBA
                uint8_t g = m_data[srcOffset];
                convertedData[dstOffset + 0] = g;
                convertedData[dstOffset + 1] = g;
                convertedData[dstOffset + 2] = g;
                convertedData[dstOffset + 3] = 255;
            } else if (m_format == PixelFormat::Grayscale && targetFormat == PixelFormat::RGB888) {
                // Gray -> RGB
                uint8_t g = m_data[srcOffset];
                convertedData[dstOffset + 0] = g;
                convertedData[dstOffset + 1] = g;
                convertedData[dstOffset + 2] = g;
            }
        }
    }

    m_data = std::move(convertedData);
    m_format = targetFormat;
    m_channels = targetChannels;
    sync();

    return PixelForgeErrorCode::SUCCESS;
}

void Image::sync() {
    width = m_width;
    height = m_height;
    channels = m_channels;
    data = m_data.empty() ? nullptr : m_data.data();
}

} // namespace PixelForge
