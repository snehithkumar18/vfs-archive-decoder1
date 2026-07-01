#include "buffered_reader.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace PixelForge {

// -----------------------------------------------------------------
// Construction / move
// -----------------------------------------------------------------

BufferedReader::BufferedReader(Stream& source, size_t buffer_size)
    : m_source(&source),
      m_buf_cap(buffer_size == 0 ? kDefaultBufferSize : buffer_size),
      m_stream_pos(source.tell()) {
    m_buffer.resize(m_buf_cap);
    m_buf_start = 0;
    m_buf_end   = 0;
}

BufferedReader::BufferedReader(BufferedReader&& o) noexcept
    : m_source(o.m_source),
      m_buffer(std::move(o.m_buffer)),
      m_buf_start(o.m_buf_start),
      m_buf_end(o.m_buf_end),
      m_buf_cap(o.m_buf_cap),
      m_stream_pos(o.m_stream_pos),
      m_bit_buf(o.m_bit_buf),
      m_bits_left(o.m_bits_left),
      m_mark_set(o.m_mark_set),
      m_mark_pos(o.m_mark_pos),
      m_mark_data(std::move(o.m_mark_data)),
      m_mark_replay_pos(o.m_mark_replay_pos),
      m_replaying(o.m_replaying) {
    o.m_source     = nullptr;
    o.m_buf_start  = 0;
    o.m_buf_end    = 0;
    o.m_bits_left  = 0;
    o.m_mark_set   = false;
    o.m_replaying  = false;
}

BufferedReader& BufferedReader::operator=(BufferedReader&& o) noexcept {
    if (this != &o) {
        m_source          = o.m_source;
        m_buffer          = std::move(o.m_buffer);
        m_buf_start       = o.m_buf_start;
        m_buf_end         = o.m_buf_end;
        m_buf_cap         = o.m_buf_cap;
        m_stream_pos      = o.m_stream_pos;
        m_bit_buf         = o.m_bit_buf;
        m_bits_left       = o.m_bits_left;
        m_mark_set        = o.m_mark_set;
        m_mark_pos        = o.m_mark_pos;
        m_mark_data       = std::move(o.m_mark_data);
        m_mark_replay_pos = o.m_mark_replay_pos;
        m_replaying       = o.m_replaying;

        o.m_source    = nullptr;
        o.m_buf_start = 0;
        o.m_buf_end   = 0;
        o.m_bits_left = 0;
        o.m_mark_set  = false;
        o.m_replaying = false;
    }
    return *this;
}

// -----------------------------------------------------------------
// Internal buffer management
// -----------------------------------------------------------------

size_t BufferedReader::refill() {
    if (!m_source) return 0;

    // If we are replaying mark data, don't touch the source
    if (m_replaying) return 0;

    // Compact: shift unconsumed data to the front of the buffer
    size_t unconsumed = m_buf_end - m_buf_start;
    if (unconsumed > 0 && m_buf_start > 0) {
        std::memmove(m_buffer.data(), m_buffer.data() + m_buf_start, unconsumed);
    }
    m_stream_pos += static_cast<int64_t>(m_buf_start);
    m_buf_start = 0;
    m_buf_end   = unconsumed;

    // Fill the rest of the buffer from the source stream
    size_t space = m_buf_cap - m_buf_end;
    if (space == 0) return 0;

    size_t n = m_source->read(m_buffer.data() + m_buf_end, space);
    m_buf_end += n;
    return n;
}

bool BufferedReader::ensure(size_t n) {
    while (buffered() < n) {
        size_t got = refill();
        if (got == 0) return false;
    }
    return true;
}

size_t BufferedReader::buffered() const {
    if (m_replaying) {
        return m_mark_data.size() - m_mark_replay_pos;
    }
    return m_buf_end - m_buf_start;
}

int64_t BufferedReader::position() const {
    if (m_replaying) {
        return m_mark_pos + static_cast<int64_t>(m_mark_replay_pos);
    }
    return m_stream_pos + static_cast<int64_t>(m_buf_start);
}

bool BufferedReader::at_end() const {
    if (m_replaying && m_mark_replay_pos < m_mark_data.size()) {
        return false;
    }
    if (m_buf_start < m_buf_end) return false;
    if (!m_source) return true;
    return m_source->eof();
}

// -----------------------------------------------------------------
// Byte-level reading
// -----------------------------------------------------------------

bool BufferedReader::read_byte(uint8_t& out) {
    // During replay, serve bytes from the mark buffer
    if (m_replaying) {
        if (m_mark_replay_pos < m_mark_data.size()) {
            out = m_mark_data[m_mark_replay_pos++];
            // When replay is exhausted, switch back to normal reading
            if (m_mark_replay_pos >= m_mark_data.size()) {
                m_replaying = false;
                // The mark data is still valid, keep accumulating
            }
            return true;
        }
        m_replaying = false;
    }

    if (m_buf_start >= m_buf_end) {
        if (refill() == 0) return false;
    }

    out = m_buffer[m_buf_start++];

    // If mark is active, record the byte
    if (m_mark_set) {
        m_mark_data.push_back(out);
    }
    return true;
}

std::vector<uint8_t> BufferedReader::read_bytes(size_t n) {
    std::vector<uint8_t> result;
    if (n == 0) return result;
    result.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        uint8_t b;
        if (!read_byte(b)) break;
        result.push_back(b);
    }

    // Caller asked for exactly n; if we got fewer, return empty
    if (result.size() != n) {
        return {};
    }
    return result;
}

bool BufferedReader::read_line(std::string& out) {
    out.clear();
    bool got_any = false;

    while (true) {
        uint8_t b;
        if (!read_byte(b)) {
            // EOF – return what we have if anything
            return got_any;
        }
        got_any = true;

        if (b == '\n') {
            // Line complete
            return true;
        }

        if (b == '\r') {
            // Check for \r\n
            uint8_t next;
            if (read_byte(next)) {
                if (next != '\n') {
                    // Not CRLF – put back by adjusting position
                    // We can't truly "unget", so we handle it:
                    // If replaying, adjust replay pos; otherwise adjust buf_start
                    if (m_replaying) {
                        if (m_mark_replay_pos > 0) {
                            --m_mark_replay_pos;
                        }
                    } else {
                        if (m_buf_start > 0) {
                            --m_buf_start;
                            // Remove from mark data if marking
                            if (m_mark_set && !m_mark_data.empty()) {
                                m_mark_data.pop_back();
                            }
                        }
                    }
                } else {
                    // Was CRLF, both consumed – nothing extra to do
                }
            }
            return true;
        }

        out.push_back(static_cast<char>(b));
    }
}

bool BufferedReader::peek(uint8_t& out) {
    if (m_replaying) {
        if (m_mark_replay_pos < m_mark_data.size()) {
            out = m_mark_data[m_mark_replay_pos];
            return true;
        }
        // Fall through to the buffer
    }

    if (m_buf_start >= m_buf_end) {
        if (refill() == 0) return false;
    }
    out = m_buffer[m_buf_start];
    return true;
}

std::vector<uint8_t> BufferedReader::peek_bytes(size_t n) {
    if (n == 0) return {};

    // For peeking, we need the data available but we must NOT advance
    // our read position.  Strategy: use mark/reset if no mark active,
    // or do a manual buffered peek.

    // Simple approach for the non-replay path: ensure we have the data
    // in the buffer and copy without advancing m_buf_start.
    if (!m_replaying) {
        // Try to get all n bytes into the buffer
        while (buffered() < n) {
            if (refill() == 0) break;
        }
        size_t avail = buffered();
        size_t to_peek = std::min(n, avail);
        return std::vector<uint8_t>(
            m_buffer.data() + m_buf_start,
            m_buffer.data() + m_buf_start + to_peek);
    }

    // During replay: peek from replay buffer
    size_t avail = m_mark_data.size() - m_mark_replay_pos;
    size_t to_peek = std::min(n, avail);
    return std::vector<uint8_t>(
        m_mark_data.data() + m_mark_replay_pos,
        m_mark_data.data() + m_mark_replay_pos + to_peek);
}

size_t BufferedReader::skip(size_t n) {
    size_t skipped = 0;
    // Fast path: skip from buffer
    while (skipped < n) {
        if (m_replaying) {
            size_t avail = m_mark_data.size() - m_mark_replay_pos;
            size_t chunk = std::min(n - skipped, avail);
            m_mark_replay_pos += chunk;
            skipped += chunk;
            if (m_mark_replay_pos >= m_mark_data.size()) {
                m_replaying = false;
            }
            if (skipped >= n) break;
        }

        size_t avail = m_buf_end - m_buf_start;
        if (avail > 0) {
            size_t chunk = std::min(n - skipped, avail);
            // If marking, record the skipped bytes
            if (m_mark_set) {
                m_mark_data.insert(m_mark_data.end(),
                    m_buffer.data() + m_buf_start,
                    m_buffer.data() + m_buf_start + chunk);
            }
            m_buf_start += chunk;
            skipped += chunk;
        } else {
            if (refill() == 0) break;
        }
    }
    return skipped;
}

// -----------------------------------------------------------------
// Bit-level reading (MSB-first)
// -----------------------------------------------------------------

bool BufferedReader::read_bit(uint8_t& out) {
    if (m_bits_left == 0) {
        // Need a fresh byte
        uint8_t b;
        if (!read_byte(b)) return false;
        m_bit_buf   = b;
        m_bits_left = 8;
    }
    // Extract the MSB
    --m_bits_left;
    out = (m_bit_buf >> m_bits_left) & 1;
    return true;
}

bool BufferedReader::read_bits(uint32_t& out, unsigned n) {
    if (n == 0) { out = 0; return true; }
    if (n > 32) return false;

    uint32_t result = 0;
    for (unsigned i = 0; i < n; ++i) {
        uint8_t bit;
        if (!read_bit(bit)) return false;
        result = (result << 1) | bit;
    }
    out = result;
    return true;
}

void BufferedReader::align_byte() {
    // Discard any partial-byte state
    m_bits_left = 0;
    m_bit_buf   = 0;
}

// -----------------------------------------------------------------
// Mark / reset
// -----------------------------------------------------------------

bool BufferedReader::mark() {
    if (m_mark_set) return false;  // only one level supported
    m_mark_set  = true;
    m_mark_pos  = position();
    m_mark_data.clear();
    m_mark_replay_pos = 0;
    m_replaying = false;
    return true;
}

bool BufferedReader::reset_to_mark() {
    if (!m_mark_set) return false;

    // We have all bytes read since the mark in m_mark_data.
    // Switch to replay mode so subsequent reads serve from there.
    m_mark_replay_pos = 0;
    m_replaying       = true;

    // Reset bit reader state too
    m_bits_left = 0;
    m_bit_buf   = 0;

    return true;
}

void BufferedReader::clear_mark() {
    m_mark_set  = false;
    m_replaying = false;
    m_mark_data.clear();
    m_mark_replay_pos = 0;
}

bool BufferedReader::has_mark() const {
    return m_mark_set;
}

} // namespace PixelForge
