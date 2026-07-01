#pragma once

#include "stream.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PixelForge {

/// Buffered writer that wraps any writable Stream.
///
/// Accumulates writes in an internal buffer and flushes to the
/// underlying stream when the buffer is full or when flush() is
/// called explicitly.  Also supports bit-level writing (MSB-first)
/// for codec use.
class BufferedWriter {
public:
    static constexpr size_t kDefaultBufferSize = 8192;

    /// Construct a buffered writer over `dest`.  The writer does NOT
    /// take ownership; the caller keeps the stream alive.
    explicit BufferedWriter(Stream& dest,
                            size_t buffer_size = kDefaultBufferSize);

    /// Destructor flushes remaining data.
    ~BufferedWriter();

    // Non-copyable, movable
    BufferedWriter(const BufferedWriter&) = delete;
    BufferedWriter& operator=(const BufferedWriter&) = delete;
    BufferedWriter(BufferedWriter&&) noexcept;
    BufferedWriter& operator=(BufferedWriter&&) noexcept;

    // -----------------------------------------------------------------
    // Byte-level writing
    // -----------------------------------------------------------------

    /// Write a single byte.
    bool write_byte(uint8_t b);

    /// Write `len` bytes from `data`.
    bool write_bytes(const void* data, size_t len);

    /// Write a string (not null-terminated unless included in str).
    bool write_string(const std::string& str);

    /// Flush the internal buffer to the underlying stream.
    bool flush();

    /// Number of bytes currently buffered but not yet flushed.
    size_t buffered() const;

    /// Logical number of bytes written since construction (including
    /// already-flushed data).
    size_t bytes_written() const;

    // -----------------------------------------------------------------
    // Bit-level writing (MSB-first)
    // -----------------------------------------------------------------

    /// Write a single bit (0 or 1), MSB-first within each byte.
    bool write_bit(uint8_t b);

    /// Write the lower `n` bits of `val` (MSB-first), 1 ≤ n ≤ 32.
    bool write_bits(uint32_t val, unsigned n);

    /// Flush any partial bit byte – pads remaining bits with zero.
    bool flush_bits();

private:
    /// Internal flush of the byte buffer.
    bool flush_buffer();

    Stream*              m_dest;
    std::vector<uint8_t> m_buffer;
    size_t               m_buf_pos   = 0;   // next write index
    size_t               m_buf_cap   = 0;
    size_t               m_total     = 0;   // total bytes committed

    // Bit writer state
    uint8_t  m_bit_buf   = 0;
    unsigned m_bit_count  = 0;  // number of bits accumulated (0..7)
};

} // namespace PixelForge
