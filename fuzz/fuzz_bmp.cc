#include "../src/bmp_codec.h"
#include "../src/image.h"
#include <cstdint>
#include <cstddef>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    PixelForge::Image image;
    PixelForge::BMPCodec::Decode(data, size, image);
    return 0;
}
