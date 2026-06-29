#include "../src/gif_codec.h"
#include <cstdint>
#include <cstddef>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) return 0;
    size_t half = size / 2;
    PixelForge::GifImage gif_image;
    PixelForge::GifCodec::Decode(data, half, gif_image);
    PixelForge::GifCodec::Decode(data + half, size - half, gif_image);
    return 0;
}
