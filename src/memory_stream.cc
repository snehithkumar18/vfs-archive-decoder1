#include "memory_stream.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace PixelForge {

// -----------------------------------------------------------------
// Constructors
// -----------------------------------------------------------------

MemoryStream::MemoryStream()
    : m_buf(std::make_shared<SharedBuffer>()),
      m_pos(0),
      m_mode(StreamMode::ReadWrite) {}

MemoryStream::MemoryStream(const std::vector<uint8_t>& data, StreamMode mode)
    : m_buf(std::make_shared<SharedBuffer>(data)),
      m_pos(0),
      m_mode(mode) {}

MemoryStream::MemoryStream(std::vector<uint8_t>&& data, StreamMode mode)
    : m_buf(std::make_shared<SharedBuffer>(std::move(data))),
      m_pos(0),
      m_mode(mode) {}

MemoryStream::MemoryStream(const uint8_t* data, size_t len, StreamMode mode)
    : m_buf(std::make_shared<SharedBuffer>()),
      m_pos(0),
      m_mode(mode) {
    if (data && len > 0) {
        m_buf->bytes.assign(data, data + len);
    }
}

// -----------------------------------------------------------------
// Move operations
// -----------------------------------------------------------------

MemoryStream::MemoryStream(MemoryStream&& other) noexcept
    : m_buf(std::move(other.m_buf)),
      m_pos(other.m_pos),
      m_mode(other.m_mode) {
    other.m_pos = 0;
    other.m_buf = std::make_shared<SharedBuffer>();
}

MemoryStream& MemoryStream::operator=(MemoryStream&& other) noexcept {
    if (this != &other) {
        m_buf  = std::move(other.m_buf);
        m_pos  = other.m_pos;
        m_mode = other.m_mode;
        other.m_pos = 0;
        other.m_buf = std::make_shared<SharedBuffer>();
    }
    return *this;
}

// -----------------------------------------------------------------
// COW detach
// -----------------------------------------------------------------

void MemoryStream::detach() {
    // If we are the sole owner nothing to do
    if (m_buf.use_count() <= 1) return;
    // Deep-copy the buffer so writes don't affect other clones
    m_buf = std::make_shared<SharedBuffer>(m_buf->bytes);
}

// -----------------------------------------------------------------
// Stream interface
// -----------------------------------------------------------------

size_t MemoryStream::read(void* buf, size_t sz) {
    if (!buf || sz == 0) return 0;
    if (m_mode == StreamMode::Write) return 0;  // not readable

    const auto& bytes = m_buf->bytes;
    if (m_pos >= bytes.size()) return 0;

    size_t avail = bytes.size() - m_pos;
    size_t to_read = std::min(sz, avail);
    std::memcpy(buf, bytes.data() + m_pos, to_read);
    m_pos += to_read;
    return to_read;
}

size_t MemoryStream::write(const void* buf, size_t sz) {
    if (!buf || sz == 0) return 0;
    if (m_mode == StreamMode::Read) return 0;  // not writable

    // Break COW sharing before mutation
    detach();

    auto& bytes = m_buf->bytes;
    size_t end_pos = m_pos + sz;

    // Grow the buffer if writing past current end
    if (end_pos > bytes.size()) {
        // Growth strategy: at least double, or exactly what is needed
        size_t new_cap = bytes.size() * 2;
        if (new_cap < end_pos) new_cap = end_pos;
        // Reserve first, then resize – avoids two allocations
        if (new_cap > bytes.capacity()) {
            bytes.reserve(new_cap);
        }
        bytes.resize(end_pos, 0);
    }

    std::memcpy(bytes.data() + m_pos, buf, sz);
    m_pos += sz;
    return sz;
}

bool MemoryStream::seek(int64_t offset, SeekOrigin whence) {
    int64_t new_pos = 0;
    const int64_t cur_size = static_cast<int64_t>(m_buf->bytes.size());

    switch (whence) {
        case SeekOrigin::Begin:
            new_pos = offset;
            break;
        case SeekOrigin::Current:
            new_pos = static_cast<int64_t>(m_pos) + offset;
            break;
        case SeekOrigin::End:
            new_pos = cur_size + offset;
            break;
    }

    if (new_pos < 0) return false;
    // Allow seeking past end – subsequent writes will fill the gap
    m_pos = static_cast<size_t>(new_pos);
    return true;
}

int64_t MemoryStream::tell() const {
    return static_cast<int64_t>(m_pos);
}

int64_t MemoryStream::size() const {
    return static_cast<int64_t>(m_buf->bytes.size());
}

bool MemoryStream::eof() const {
    return m_pos >= m_buf->bytes.size();
}

bool MemoryStream::is_readable() const {
    return m_mode == StreamMode::Read || m_mode == StreamMode::ReadWrite;
}

bool MemoryStream::is_writable() const {
    return m_mode == StreamMode::Write || m_mode == StreamMode::ReadWrite;
}

bool MemoryStream::is_seekable() const {
    return true;  // Memory streams are always seekable
}

// -----------------------------------------------------------------
// MemoryStream-specific API
// -----------------------------------------------------------------

MemoryStream MemoryStream::clone() const {
    MemoryStream c;
    c.m_buf  = m_buf;    // shallow – shared_ptr refcount bumps
    c.m_pos  = m_pos;
    c.m_mode = m_mode;
    return c;
}

const std::vector<uint8_t>& MemoryStream::get_data() const {
    return m_buf->bytes;
}

std::vector<uint8_t> MemoryStream::release_data() {
    detach();  // ensure unique ownership before moving
    auto data = std::move(m_buf->bytes);
    m_pos = 0;
    return data;
}

void MemoryStream::resize(size_t new_size) {
    detach();
    m_buf->bytes.resize(new_size, 0);
    if (m_pos > new_size) {
        m_pos = new_size;
    }
}

void MemoryStream::reserve(size_t cap) {
    detach();
    m_buf->bytes.reserve(cap);
}

void MemoryStream::clear() {
    detach();
    m_buf->bytes.clear();
    m_pos = 0;
}

const uint8_t* MemoryStream::raw_data() const {
    return m_buf->bytes.data();
}

size_t MemoryStream::capacity() const {
    return m_buf->bytes.capacity();
}

} // namespace PixelForge
