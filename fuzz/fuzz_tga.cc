#include "../src/tga_codec.h"
#include "../src/image.h"
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) return 0;
    size_t half = size / 2;
    PixelForge::Image image;
    PixelForge::TGACodec::Decode(data, half, image);
    PixelForge::TGACodec::Decode(data + half, size - half, image);
    return 0;
}
