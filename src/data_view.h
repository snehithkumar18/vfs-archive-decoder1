#pragma once

#include "pf_endian.h"
#include "errors.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace PixelForge {

class MemoryStream;  // forward declaration

/// Non-owning, bounds-checked view over a contiguous byte range.
///
/// DataView never copies data and never takes ownership.  It is the
/// caller's responsibility to keep the underlying memory alive for
/// the lifetime of the DataView.
class DataView {
public:
    // ---- iterators ------------------------------------------------
    using iterator       = const uint8_t*;
    using const_iterator = const uint8_t*;

    // ---- constructors ---------------------------------------------

    /// Empty view.
    constexpr DataView() noexcept = default;

    /// View over [data, data + size).
    DataView(const uint8_t* data, size_t size) noexcept;

    /// View over a vector's contents.
    explicit DataView(const std::vector<uint8_t>& vec) noexcept;

    /// View from a MemoryStream (reads current data snapshot).
    explicit DataView(const MemoryStream& ms) noexcept;

    // ---- accessors ------------------------------------------------

    const uint8_t* data() const noexcept { return m_data; }
    size_t         size() const noexcept { return m_size; }
    bool           empty() const noexcept { return m_size == 0; }

    /// Unchecked element access.
    uint8_t operator[](size_t idx) const { return m_data[idx]; }

    /// Checked element access – returns false on out-of-bounds.
    bool at(size_t idx, uint8_t& out) const;

    // ---- slicing --------------------------------------------------

    /// Return a sub-view starting at `offset` with `len` bytes.
    /// Returns an empty view if the range is out-of-bounds.
    DataView slice(size_t offset, size_t len) const;

    /// Convenience: slice from offset to end.
    DataView slice_from(size_t offset) const;

    // ---- typed reads (bounds-checked) -----------------------------

    bool read_u8(size_t offset, uint8_t& out) const;
    bool read_i8(size_t offset, int8_t& out) const;
    bool read_u16_le(size_t offset, uint16_t& out) const;
    bool read_u16_be(size_t offset, uint16_t& out) const;
    bool read_u32_le(size_t offset, uint32_t& out) const;
    bool read_u32_be(size_t offset, uint32_t& out) const;
    bool read_i32_le(size_t offset, int32_t& out) const;
    bool read_i32_be(size_t offset, int32_t& out) const;
    bool read_u64_le(size_t offset, uint64_t& out) const;
    bool read_u64_be(size_t offset, uint64_t& out) const;

    /// Copy `len` bytes starting at `offset` into `dst`.
    bool read_bytes(size_t offset, void* dst, size_t len) const;

    /// Compare `len` bytes at `offset` against `expected`.
    bool equals(size_t offset, const void* expected, size_t len) const;

    // ---- searching ------------------------------------------------

    /// Find the first occurrence of byte `b` starting at `from`.
    /// Returns the offset, or `size()` if not found.
    size_t find(uint8_t b, size_t from = 0) const;

    /// Find the first occurrence of a byte sequence.
    size_t find(const uint8_t* needle, size_t needle_len,
                size_t from = 0) const;

    // ---- iterators ------------------------------------------------

    const_iterator begin() const noexcept { return m_data; }
    const_iterator end()   const noexcept { return m_data + m_size; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend()   const noexcept { return end(); }

private:
    const uint8_t* m_data = nullptr;
    size_t         m_size = 0;
};

} // namespace PixelForge
