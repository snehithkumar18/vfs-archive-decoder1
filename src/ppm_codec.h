#ifndef PIXELFORGE_PPM_CODEC_H
#define PIXELFORGE_PPM_CODEC_H

#include "image.h"
#include "errors.h"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace PixelForge {

class PPMCodec {
public:
    // Decode PBM/PGM/PPM data into Image
    static PixelForgeErrorCode Decode(const uint8_t* data, size_t size, Image& out_image);
    static PixelForgeErrorCode Decode(const std::vector<uint8_t>& data, Image& out_image);

    // Encode Image into PPM P6 (binary true-color)
    static PixelForgeErrorCode EncodeP6(const Image& image, std::vector<uint8_t>& out_data);

    // Encode Image into PGM P5 (binary grayscale)
    static PixelForgeErrorCode EncodeP5(const Image& image, std::vector<uint8_t>& out_data);
};

} // namespace PixelForge

#endif // PIXELFORGE_PPM_CODEC_H
