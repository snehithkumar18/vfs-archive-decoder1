#include "../src/gif_codec.h"
#include <cstdint>
#include <cstddef>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    PixelForge::GifImage gif_image;
    if (PixelForge::GifCodec::Decode(data, size, gif_image)) {
        if (!gif_image.frames.empty()) {
            std::vector<PixelForge::Color> rgba;
            PixelForge::GifCodec::RenderFrameRGBA(gif_image, 0, rgba);
        }
    }
    return 0;
}
