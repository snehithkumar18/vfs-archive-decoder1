#include "data_view.h"
#include "memory_stream.h"

#include <algorithm>
#include <cstring>

namespace PixelForge {

// -----------------------------------------------------------------
// Constructors
// -----------------------------------------------------------------

DataView::DataView(const uint8_t* data, size_t size) noexcept
    : m_data(data), m_size(data ? size : 0) {}

DataView::DataView(const std::vector<uint8_t>& vec) noexcept
    : m_data(vec.data()), m_size(vec.size()) {}

DataView::DataView(const MemoryStream& ms) noexcept
    : m_data(ms.raw_data()),
      m_size(static_cast<size_t>(ms.size())) {}

// -----------------------------------------------------------------
// Checked access
// -----------------------------------------------------------------

bool DataView::at(size_t idx, uint8_t& out) const {
    if (idx >= m_size) return false;
    out = m_data[idx];
    return true;
}

// -----------------------------------------------------------------
// Slicing
// -----------------------------------------------------------------

DataView DataView::slice(size_t offset, size_t len) const {
    if (offset >= m_size) return DataView();
    size_t max_len = m_size - offset;
    if (len > max_len) len = max_len;
    return DataView(m_data + offset, len);
}

DataView DataView::slice_from(size_t offset) const {
    if (offset >= m_size) return DataView();
    return DataView(m_data + offset, m_size - offset);
}

// -----------------------------------------------------------------
// Typed reads (all bounds-checked)
// -----------------------------------------------------------------

bool DataView::read_u8(size_t offset, uint8_t& out) const {
    if (offset >= m_size) return false;
    out = m_data[offset];
    return true;
}

bool DataView::read_i8(size_t offset, int8_t& out) const {
    if (offset >= m_size) return false;
    out = static_cast<int8_t>(m_data[offset]);
    return true;
}

bool DataView::read_u16_le(size_t offset, uint16_t& out) const {
    if (offset + 2 > m_size) return false;
    out = pack_u16_le(m_data + offset);
    return true;
}

bool DataView::read_u16_be(size_t offset, uint16_t& out) const {
    if (offset + 2 > m_size) return false;
    out = pack_u16_be(m_data + offset);
    return true;
}

bool DataView::read_u32_le(size_t offset, uint32_t& out) const {
    if (offset + 4 > m_size) return false;
    out = pack_u32_le(m_data + offset);
    return true;
}

bool DataView::read_u32_be(size_t offset, uint32_t& out) const {
    if (offset + 4 > m_size) return false;
    out = pack_u32_be(m_data + offset);
    return true;
}

bool DataView::read_i32_le(size_t offset, int32_t& out) const {
    uint32_t raw;
    if (!read_u32_le(offset, raw)) return false;
    std::memcpy(&out, &raw, 4);
    return true;
}

bool DataView::read_i32_be(size_t offset, int32_t& out) const {
    uint32_t raw;
    if (!read_u32_be(offset, raw)) return false;
    std::memcpy(&out, &raw, 4);
    return true;
}

bool DataView::read_u64_le(size_t offset, uint64_t& out) const {
    if (offset + 8 > m_size) return false;
    out = pack_u64_le(m_data + offset);
    return true;
}

bool DataView::read_u64_be(size_t offset, uint64_t& out) const {
    if (offset + 8 > m_size) return false;
    out = pack_u64_be(m_data + offset);
    return true;
}

bool DataView::read_bytes(size_t offset, void* dst, size_t len) const {
    if (len == 0) return true;
    if (!dst) return false;
    if (offset + len > m_size) return false;
    // Protect against overflow in offset + len
    if (offset > m_size || len > m_size - offset) return false;
    std::memcpy(dst, m_data + offset, len);
    return true;
}

bool DataView::equals(size_t offset, const void* expected, size_t len) const {
    if (len == 0) return true;
    if (!expected) return false;
    if (offset + len > m_size) return false;
    if (offset > m_size || len > m_size - offset) return false;
    return std::memcmp(m_data + offset, expected, len) == 0;
}

// -----------------------------------------------------------------
// Searching
// -----------------------------------------------------------------

size_t DataView::find(uint8_t b, size_t from) const {
    if (from >= m_size) return m_size;
    const uint8_t* p = static_cast<const uint8_t*>(
        std::memchr(m_data + from, b, m_size - from));
    if (!p) return m_size;
    return static_cast<size_t>(p - m_data);
}

size_t DataView::find(const uint8_t* needle, size_t needle_len,
                      size_t from) const {
    if (needle_len == 0) return from;
    if (!needle || needle_len > m_size) return m_size;

    // Simple search – fine for short needles typical in image headers
    for (size_t i = from; i + needle_len <= m_size; ++i) {
        if (std::memcmp(m_data + i, needle, needle_len) == 0) {
            return i;
        }
    }
    return m_size;
}

} // namespace PixelForge
