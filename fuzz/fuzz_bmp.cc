#include "../src/bmp_codec.h"
#include "../src/image.h"
#include <cstdint>
#include <cstddef>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 60) return 0;
    
    // Split the input into two potential BMPs
    // First 4 bytes determine the split offset
    uint32_t split_offset = 0;
    std::memcpy(&split_offset, data, 4);
    split_offset = split_offset % (size - 4);
    
    if (split_offset >= 54 && split_offset <= size - 54) {
        {
            PixelForge::Image image1;
            PixelForge::BMPCodec::Decode(data + 4, split_offset, image1);
        } // image1 is destroyed here, freeing its pixel buffer
        
        PixelForge::Image image2;
        PixelForge::BMPCodec::Decode(data + 4 + split_offset, size - 4 - split_offset, image2);
    } else {
        PixelForge::Image image;
        PixelForge::BMPCodec::Decode(data, size, image);
    }
    
    return 0;
}
