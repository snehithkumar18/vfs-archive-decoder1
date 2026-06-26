#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint>
#include <cstddef>
#include <vector>

class VFSDecompressor {
public:
    virtual ~VFSDecompressor() = default;
    
    // Abstract interface for decompression
    virtual bool decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) = 0;
};

class RLEDecompressor : public VFSDecompressor {
public:
    // Bug 2 resides in this implementation
    bool decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) override;
};

class HuffmanDecompressor : public VFSDecompressor {
private:
    struct HuffmanNode {
        uint8_t value;
        bool is_leaf;
        HuffmanNode* left;
        HuffmanNode* right;
        
        HuffmanNode() : value(0), is_leaf(false), left(nullptr), right(nullptr) {}
        ~HuffmanNode() {
            delete left;
            delete right;
        }
    };
    
    HuffmanNode* build_tree_from_frequencies(const std::vector<uint32_t>& frequencies);

public:
    bool decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) override;
};

class LZWDecompressor : public VFSDecompressor {
public:
    bool decompress(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) override;
};

#endif // COMPRESSION_H
