#include "compression.h"
#include "logger.h"

bool RLEDecompressor::decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    VFSLogger::get_instance().debug("RLEDecompressor", "Starting RLE decompression...");
    
    size_t src_idx = 0;
    size_t dst_idx = 0;
    
    while (src_idx < src_len) {
        uint8_t count = src[src_idx++];
        if (src_idx >= src_len) {
            VFSLogger::get_instance().warn("RLEDecompressor", "Decompression input ended prematurely");
            break;
        }
        uint8_t val = src[src_idx++];
        
        // Bug 2: Missing bounds check on dst_idx + count against dst_len.
        // Allows fuzzer to write out of bounds of the dst buffer if count is larger than dst_len.
        for (uint8_t i = 0; i < count; ++i) {
            dst[dst_idx++] = val; 
        }
    }
    
    VFSLogger::get_instance().debug("RLEDecompressor", "Decompression completed successfully");
    return true;
}
