#include "compression.h"
#include "logger.h"
#include <queue>
#include <vector>

struct HuffmanTreeCompare {
    typedef HuffmanDecompressor::HuffmanNode Node;
    bool operator()(const std::pair<uint32_t, Node*>& lhs, const std::pair<uint32_t, Node*>& rhs) const {
        return lhs.first > rhs.first;
    }
};

HuffmanDecompressor::HuffmanNode* HuffmanDecompressor::build_tree_from_frequencies(const std::vector<uint32_t>& frequencies) {
    std::priority_queue<std::pair<uint32_t, HuffmanNode*>, 
                       std::vector<std::pair<uint32_t, HuffmanNode*>>, 
                       HuffmanTreeCompare> pq;
                       
    for (size_t i = 0; i < frequencies.size(); ++i) {
        if (frequencies[i] > 0) {
            HuffmanNode* node = new HuffmanNode();
            node->value = (uint8_t)i;
            node->is_leaf = true;
            pq.push({frequencies[i], node});
        }
    }
    
    if (pq.empty()) return nullptr;
    
    while (pq.size() > 1) {
        auto left_pair = pq.top(); pq.pop();
        auto right_pair = pq.top(); pq.pop();
        
        HuffmanNode* parent = new HuffmanNode();
        parent->is_leaf = false;
        parent->left = left_pair.second;
        parent->right = right_pair.second;
        
        pq.push({left_pair.first + right_pair.first, parent});
    }
    
    return pq.top().second;
}

bool HuffmanDecompressor::decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    VFSLogger::get_instance().debug("HuffmanDecompressor", "Starting Huffman decompression...");
    
    // Check minimal size requirements
    // Format: 256 uint32 frequency table (1024 bytes) + bitstream
    if (src_len < 1024) {
        VFSLogger::get_instance().error("HuffmanDecompressor", "Input too small for Huffman table header");
        return false;
    }
    
    std::vector<uint32_t> frequencies(256, 0);
    for (int i = 0; i < 256; ++i) {
        uint32_t freq = ((uint32_t)src[i*4]) |
                        ((uint32_t)src[i*4+1] << 8) |
                        ((uint32_t)src[i*4+2] << 16) |
                        ((uint32_t)src[i*4+3] << 24);
        frequencies[i] = freq;
    }
    
    HuffmanNode* root = build_tree_from_frequencies(frequencies);
    if (!root) {
        VFSLogger::get_instance().error("HuffmanDecompressor", "Failed to build Huffman Tree");
        return false;
    }
    
    size_t src_bit_idx = 1024 * 8;
    size_t dst_idx = 0;
    
    HuffmanNode* curr = root;
    while (dst_idx < dst_len && (src_bit_idx / 8) < src_len) {
        size_t byte_idx = src_bit_idx / 8;
        size_t bit_offset = 7 - (src_bit_idx % 8);
        bool bit = (src[byte_idx] >> bit_offset) & 1;
        src_bit_idx++;
        
        if (bit) {
            curr = curr->right;
        } else {
            curr = curr->left;
        }
        
        if (!curr) {
            VFSLogger::get_instance().error("HuffmanDecompressor", "Traversed into invalid tree path");
            delete root;
            return false;
        }
        
        if (curr->is_leaf) {
            dst[dst_idx++] = curr->value;
            curr = root;
        }
    }
    
    delete root;
    VFSLogger::get_instance().debug("HuffmanDecompressor", "Huffman Decompression successfully finished");
    return true;
}
