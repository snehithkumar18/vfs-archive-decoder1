#pragma once

#include "stream.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PixelForge {

/// High-performance buffered reader that wraps any Stream.
///
/// Provides byte-level, line-level, and bit-level reading with an
/// internal refill buffer.  Supports mark/reset for look-ahead parsing
/// and peek operations without consuming data.
class BufferedReader {
public:
    static constexpr size_t kDefaultBufferSize = 8192;

    /// Construct a buffered reader over `source`.  The reader does NOT
    /// take ownership of the stream; the caller must keep it alive.
    explicit BufferedReader(Stream& source,
                            size_t buffer_size = kDefaultBufferSize);

    ~BufferedReader() = default;

    // Non-copyable, movable
    BufferedReader(const BufferedReader&) = delete;
    BufferedReader& operator=(const BufferedReader&) = delete;
    BufferedReader(BufferedReader&&) noexcept;
    BufferedReader& operator=(BufferedReader&&) noexcept;

    // -----------------------------------------------------------------
    // Byte-level reading
    // -----------------------------------------------------------------

    /// Read a single byte.  Returns false on EOF/error.
    bool read_byte(uint8_t& out);

    /// Read exactly `n` bytes into the returned vector.
    /// Returns an empty vector if fewer than `n` bytes are available.
    std::vector<uint8_t> read_bytes(size_t n);

    /// Read a line terminated by '\n' (LF) or '\r\n' (CRLF).
    /// The terminator is consumed but NOT included in the output string.
    /// Returns false at EOF when no bytes were read.
    bool read_line(std::string& out);

    /// Peek at the next byte without consuming it.
    bool peek(uint8_t& out);

    /// Peek at the next `n` bytes without consuming them.
    /// Returns fewer bytes if not enough data is available.
    std::vector<uint8_t> peek_bytes(size_t n);

    /// Skip `n` bytes.  Returns the number of bytes actually skipped.
    size_t skip(size_t n);

    /// Number of unconsumed bytes currently buffered.
    size_t buffered() const;

    /// Logical stream position accounting for buffering.
    int64_t position() const;

    /// True when the underlying stream is exhausted and the buffer is
    /// empty.
    bool at_end() const;

    // -----------------------------------------------------------------
    // Bit-level reading (MSB-first, used by Huffman / entropy codecs)
    // -----------------------------------------------------------------

    /// Read a single bit (MSB first within each byte).
    /// Returns false on EOF.
    bool read_bit(uint8_t& out);

    /// Read `n` bits (1..32) into `out`, MSB-first.
    /// Returns false if there aren't enough bits.
    bool read_bits(uint32_t& out, unsigned n);

    /// Discard remaining bits in the current byte so the next read
    /// starts at a byte boundary.
    void align_byte();

    // -----------------------------------------------------------------
    // Mark / reset (single-level)
    // -----------------------------------------------------------------

    /// Set a mark at the current logical position.
    bool mark();

    /// Reset the read position back to the mark.
    bool reset_to_mark();

    /// Clear any previously set mark.
    void clear_mark();

    /// True if a mark is currently set.
    bool has_mark() const;

private:
    /// Refill the internal buffer from the source stream.
    /// Returns the number of bytes newly buffered.
    size_t refill();

    /// Ensure at least `n` bytes are available in the buffer.
    /// Returns false if fewer than `n` could be buffered.
    bool ensure(size_t n);

    Stream*              m_source;
    std::vector<uint8_t> m_buffer;
    size_t               m_buf_start  = 0;  // index of first valid byte
    size_t               m_buf_end    = 0;  // index one past last valid byte
    size_t               m_buf_cap    = 0;  // allocated capacity
    int64_t              m_stream_pos = 0;  // position in source at m_buf_start

    // Bit reader state
    uint8_t  m_bit_buf   = 0;
    unsigned m_bits_left = 0;  // bits remaining in m_bit_buf

    // Mark/reset state
    bool    m_mark_set = false;
    int64_t m_mark_pos = 0;      // logical position of the mark
    // When a mark is set we accumulate all read bytes so we can replay
    std::vector<uint8_t> m_mark_data;
    size_t               m_mark_replay_pos = 0;  // index into mark_data during replay
    bool                 m_replaying       = false;
};

} // namespace PixelForge
