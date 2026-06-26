#include "compression.h"
#include "logger.h"
#include <map>
#include <string>
#include <vector>

bool LZWDecompressor::decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    VFSLogger::get_instance().debug("LZWDecompressor", "Starting LZW decompression...");
    
    if (src_len == 0) return true;
    
    // Initialize dictionary with single character strings
    std::map<uint32_t, std::vector<uint8_t>> dictionary;
    for (int i = 0; i < 256; ++i) {
        dictionary[i] = std::vector<uint8_t>{(uint8_t)i};
    }
    
    uint32_t dict_size = 256;
    const uint32_t MAX_DICT_SIZE = 4096;
    
    // Parse 12-bit codes from the input stream
    std::vector<uint32_t> codes;
    size_t bit_idx = 0;
    
    while ((bit_idx + 12) <= src_len * 8) {
        uint32_t code = 0;
        for (int i = 0; i < 12; ++i) {
            size_t total_bit = bit_idx + i;
            size_t byte_idx = total_bit / 8;
            size_t bit_offset = 7 - (total_bit % 8);
            bool bit = (src[byte_idx] >> bit_offset) & 1;
            code = (code << 1) | (bit ? 1 : 0);
        }
        codes.push_back(code);
        bit_idx += 12;
    }
    
    if (codes.empty()) return true;
    
    uint32_t old_code = codes[0];
    if (dictionary.find(old_code) == dictionary.end()) {
        VFSLogger::get_instance().error("LZWDecompressor", "First code is not in dictionary");
        return false;
    }
    
    std::vector<uint8_t> val = dictionary[old_code];
    size_t dst_idx = 0;
    
    // Write first entry
    for (uint8_t b : val) {
        if (dst_idx >= dst_len) break;
        dst[dst_idx++] = b;
    }
    
    for (size_t i = 1; i < codes.size(); ++i) {
        uint32_t new_code = codes[i];
        std::vector<uint8_t> entry;
        
        if (dictionary.find(new_code) != dictionary.end()) {
            entry = dictionary[new_code];
        } else if (new_code == dict_size) {
            entry = dictionary[old_code];
            entry.push_back(val[0]);
        } else {
            VFSLogger::get_instance().error("LZWDecompressor", "Invalid LZW compressed code");
            return false;
        }
        
        // Write entry
        for (uint8_t b : entry) {
            if (dst_idx >= dst_len) break;
            dst[dst_idx++] = b;
        }
        
        // Add to dictionary if space is available
        if (dict_size < MAX_DICT_SIZE) {
            std::vector<uint8_t> new_dict_entry = dictionary[old_code];
            new_dict_entry.push_back(entry[0]);
            dictionary[dict_size++] = new_dict_entry;
        }
        
        val = entry;
        old_code = new_code;
    }
    
    VFSLogger::get_instance().debug("LZWDecompressor", "LZW decompression completed successfully");
    return true;
}
