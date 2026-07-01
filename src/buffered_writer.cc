#include "buffered_writer.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace PixelForge {

// -----------------------------------------------------------------
// Construction / destruction / move
// -----------------------------------------------------------------

BufferedWriter::BufferedWriter(Stream& dest, size_t buffer_size)
    : m_dest(&dest),
      m_buf_cap(buffer_size == 0 ? kDefaultBufferSize : buffer_size),
      m_buf_pos(0),
      m_total(0),
      m_bit_buf(0),
      m_bit_count(0) {
    m_buffer.resize(m_buf_cap);
}

BufferedWriter::~BufferedWriter() {
    // Flush any remaining bits and buffered bytes on destruction.
    // We ignore errors here because destructors must not throw.
    if (m_bit_count > 0) {
        flush_bits();
    }
    if (m_buf_pos > 0) {
        flush_buffer();
    }
}

BufferedWriter::BufferedWriter(BufferedWriter&& o) noexcept
    : m_dest(o.m_dest),
      m_buffer(std::move(o.m_buffer)),
      m_buf_pos(o.m_buf_pos),
      m_buf_cap(o.m_buf_cap),
      m_total(o.m_total),
      m_bit_buf(o.m_bit_buf),
      m_bit_count(o.m_bit_count) {
    o.m_dest      = nullptr;
    o.m_buf_pos   = 0;
    o.m_total     = 0;
    o.m_bit_count = 0;
}

BufferedWriter& BufferedWriter::operator=(BufferedWriter&& o) noexcept {
    if (this != &o) {
        // Flush current data first
        if (m_bit_count > 0) flush_bits();
        if (m_buf_pos > 0)   flush_buffer();

        m_dest      = o.m_dest;
        m_buffer    = std::move(o.m_buffer);
        m_buf_pos   = o.m_buf_pos;
        m_buf_cap   = o.m_buf_cap;
        m_total     = o.m_total;
        m_bit_buf   = o.m_bit_buf;
        m_bit_count = o.m_bit_count;

        o.m_dest      = nullptr;
        o.m_buf_pos   = 0;
        o.m_total     = 0;
        o.m_bit_count = 0;
    }
    return *this;
}

// -----------------------------------------------------------------
// Internal flush
// -----------------------------------------------------------------

bool BufferedWriter::flush_buffer() {
    if (m_buf_pos == 0) return true;
    if (!m_dest) return false;

    size_t written = 0;
    while (written < m_buf_pos) {
        size_t n = m_dest->write(m_buffer.data() + written,
                                 m_buf_pos - written);
        if (n == 0) return false;  // underlying stream refused the write
        written += n;
    }
    m_buf_pos = 0;
    return true;
}

// -----------------------------------------------------------------
// Byte-level writing
// -----------------------------------------------------------------

bool BufferedWriter::write_byte(uint8_t b) {
    if (m_buf_pos >= m_buf_cap) {
        if (!flush_buffer()) return false;
    }
    m_buffer[m_buf_pos++] = b;
    ++m_total;
    return true;
}

bool BufferedWriter::write_bytes(const void* data, size_t len) {
    if (!data || len == 0) return true;

    auto* src = static_cast<const uint8_t*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        size_t space = m_buf_cap - m_buf_pos;

        if (remaining >= m_buf_cap && m_buf_pos == 0) {
            // Large write: bypass the buffer entirely to avoid a copy.
            // Write directly to the underlying stream.
            size_t written = 0;
            while (written < remaining) {
                size_t n = m_dest->write(src + written, remaining - written);
                if (n == 0) return false;
                written += n;
            }
            m_total += remaining;
            return true;
        }

        if (space == 0) {
            if (!flush_buffer()) return false;
            space = m_buf_cap;
        }

        size_t chunk = std::min(remaining, space);
        std::memcpy(m_buffer.data() + m_buf_pos, src, chunk);
        m_buf_pos  += chunk;
        src        += chunk;
        remaining  -= chunk;
        m_total    += chunk;
    }
    return true;
}

bool BufferedWriter::write_string(const std::string& str) {
    if (str.empty()) return true;
    return write_bytes(str.data(), str.size());
}

bool BufferedWriter::flush() {
    // Flush partial bits first, then bytes
    if (m_bit_count > 0) {
        if (!flush_bits()) return false;
    }
    return flush_buffer();
}

size_t BufferedWriter::buffered() const {
    return m_buf_pos;
}

size_t BufferedWriter::bytes_written() const {
    return m_total;
}

// -----------------------------------------------------------------
// Bit-level writing (MSB-first)
// -----------------------------------------------------------------

bool BufferedWriter::write_bit(uint8_t b) {
    m_bit_buf = static_cast<uint8_t>((m_bit_buf << 1) | (b & 1));
    ++m_bit_count;

    if (m_bit_count == 8) {
        if (!write_byte(m_bit_buf)) return false;
        m_bit_buf   = 0;
        m_bit_count = 0;
    }
    return true;
}

bool BufferedWriter::write_bits(uint32_t val, unsigned n) {
    if (n == 0) return true;
    if (n > 32) return false;

    // Write bits MSB-first: the most significant of the n bits first
    for (unsigned i = n; i > 0; --i) {
        uint8_t bit = static_cast<uint8_t>((val >> (i - 1)) & 1);
        if (!write_bit(bit)) return false;
    }
    return true;
}

bool BufferedWriter::flush_bits() {
    if (m_bit_count == 0) return true;
    // Pad remaining bits with zeros to complete the byte
    unsigned pad = 8 - m_bit_count;
    m_bit_buf = static_cast<uint8_t>(m_bit_buf << pad);
    m_bit_count = 0;
    bool ok = write_byte(m_bit_buf);
    m_bit_buf = 0;
    return ok;
}

} // namespace PixelForge
