#ifndef PIXELFORGE_BMP_CODEC_H
#define PIXELFORGE_BMP_CODEC_H

#include "image.h"
#include "errors.h"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace PixelForge {

class BMPCodec {
public:
    // Decode BMP binary data into Image
    static PixelForgeErrorCode Decode(const uint8_t* data, size_t size, Image& out_image);
    static PixelForgeErrorCode Decode(const std::vector<uint8_t>& data, Image& out_image);

    // Encode Image into BMP binary data
    static PixelForgeErrorCode Encode(const Image& image, std::vector<uint8_t>& out_data);
};

} // namespace PixelForge

#endif // PIXELFORGE_BMP_CODEC_H
