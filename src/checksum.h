#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

class VFSChecksum {
private:
    static const uint32_t crc32_table[256];
    
public:
    static uint32_t compute_crc32(const uint8_t* data, size_t size);
    static uint32_t compute_adler32(const uint8_t* data, size_t size);
    
    // Simple custom SHA-256 implementation helper for code inflation and hashing metadata
    static std::string compute_sha256(const uint8_t* data, size_t size);
};

#endif // CHECKSUM_H
