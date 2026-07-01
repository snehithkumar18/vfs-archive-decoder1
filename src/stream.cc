#include "stream.h"
#include "pf_endian.h"

#include <cstring>

namespace PixelForge {

// -----------------------------------------------------------------
// read helpers
// -----------------------------------------------------------------

bool Stream::read_u8(uint8_t& out) {
    return read_exact(&out, 1);
}

bool Stream::read_u16_le(uint16_t& out) {
    uint8_t buf[2];
    if (!read_exact(buf, 2)) return false;
    out = pack_u16_le(buf);
    return true;
}

bool Stream::read_u16_be(uint16_t& out) {
    uint8_t buf[2];
    if (!read_exact(buf, 2)) return false;
    out = pack_u16_be(buf);
    return true;
}

bool Stream::read_u32_le(uint32_t& out) {
    uint8_t buf[4];
    if (!read_exact(buf, 4)) return false;
    out = pack_u32_le(buf);
    return true;
}

bool Stream::read_u32_be(uint32_t& out) {
    uint8_t buf[4];
    if (!read_exact(buf, 4)) return false;
    out = pack_u32_be(buf);
    return true;
}

bool Stream::read_i32_le(int32_t& out) {
    uint32_t raw;
    if (!read_u32_le(raw)) return false;
    std::memcpy(&out, &raw, 4);
    return true;
}

bool Stream::read_i32_be(int32_t& out) {
    uint32_t raw;
    if (!read_u32_be(raw)) return false;
    std::memcpy(&out, &raw, 4);
    return true;
}

bool Stream::read_u64_le(uint64_t& out) {
    uint8_t buf[8];
    if (!read_exact(buf, 8)) return false;
    out = pack_u64_le(buf);
    return true;
}

bool Stream::read_u64_be(uint64_t& out) {
    uint8_t buf[8];
    if (!read_exact(buf, 8)) return false;
    out = pack_u64_be(buf);
    return true;
}

bool Stream::read_exact(void* buf, size_t count) {
    if (count == 0) return true;
    size_t total = 0;
    auto* dst = static_cast<uint8_t*>(buf);
    while (total < count) {
        size_t n = read(dst + total, count - total);
        if (n == 0) return false;  // EOF or error before finishing
        total += n;
    }
    return true;
}

bool Stream::skip(size_t count) {
    if (count == 0) return true;
    // Try seeking first – much faster on seekable streams
    if (is_seekable()) {
        int64_t cur = tell();
        if (cur >= 0) {
            return seek(static_cast<int64_t>(count), SeekOrigin::Current);
        }
    }
    // Fallback: read and discard
    uint8_t discard[4096];
    size_t left = count;
    while (left > 0) {
        size_t chunk = (left < sizeof(discard)) ? left : sizeof(discard);
        size_t n = read(discard, chunk);
        if (n == 0) return false;
        left -= n;
    }
    return true;
}

bool Stream::read_string(std::string& out, size_t length) {
    out.resize(length);
    if (length == 0) return true;
    return read_exact(&out[0], length);
}

// -----------------------------------------------------------------
// write helpers
// -----------------------------------------------------------------

bool Stream::write_u8(uint8_t val) {
    return write_exact(&val, 1);
}

bool Stream::write_u16_le(uint16_t val) {
    uint8_t buf[2];
    unpack_u16_le(val, buf);
    return write_exact(buf, 2);
}

bool Stream::write_u16_be(uint16_t val) {
    uint8_t buf[2];
    unpack_u16_be(val, buf);
    return write_exact(buf, 2);
}

bool Stream::write_u32_le(uint32_t val) {
    uint8_t buf[4];
    unpack_u32_le(val, buf);
    return write_exact(buf, 4);
}

bool Stream::write_u32_be(uint32_t val) {
    uint8_t buf[4];
    unpack_u32_be(val, buf);
    return write_exact(buf, 4);
}

bool Stream::write_i32_le(int32_t val) {
    uint32_t raw;
    std::memcpy(&raw, &val, 4);
    return write_u32_le(raw);
}

bool Stream::write_i32_be(int32_t val) {
    uint32_t raw;
    std::memcpy(&raw, &val, 4);
    return write_u32_be(raw);
}

bool Stream::write_u64_le(uint64_t val) {
    uint8_t buf[8];
    unpack_u64_le(val, buf);
    return write_exact(buf, 8);
}

bool Stream::write_u64_be(uint64_t val) {
    uint8_t buf[8];
    unpack_u64_be(val, buf);
    return write_exact(buf, 8);
}

bool Stream::write_exact(const void* buf, size_t count) {
    if (count == 0) return true;
    size_t total = 0;
    auto* src = static_cast<const uint8_t*>(buf);
    while (total < count) {
        size_t n = write(src + total, count - total);
        if (n == 0) return false;  // cannot write further
        total += n;
    }
    return true;
}

bool Stream::write_string(const std::string& str) {
    if (str.empty()) return true;
    return write_exact(str.data(), str.size());
}

} // namespace PixelForge
