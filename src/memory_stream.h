#pragma once

#include "stream.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace PixelForge {

/// In-memory stream backed by a growable std::vector<uint8_t>.
///
/// Supports read, write, and seek.  Provides copy-on-write semantics
/// through clone(): the cloned stream shares the underlying buffer
/// until either copy writes, at which point a deep copy is made.
class MemoryStream final : public Stream {
public:
    /// Construct an empty read/write memory stream.
    MemoryStream();

    /// Construct a memory stream over an existing buffer (copied).
    /// The stream is positioned at byte 0.
    explicit MemoryStream(const std::vector<uint8_t>& data,
                          StreamMode mode = StreamMode::ReadWrite);

    /// Move-construct from an existing buffer (zero-copy).
    explicit MemoryStream(std::vector<uint8_t>&& data,
                          StreamMode mode = StreamMode::ReadWrite);

    /// Construct from a raw pointer + size (data is copied in).
    MemoryStream(const uint8_t* data, size_t len,
                 StreamMode mode = StreamMode::ReadWrite);

    ~MemoryStream() override = default;

    // Move operations
    MemoryStream(MemoryStream&& other) noexcept;
    MemoryStream& operator=(MemoryStream&& other) noexcept;

    // Stream interface
    size_t read(void* buf, size_t sz) override;
    size_t write(const void* buf, size_t sz) override;
    bool   seek(int64_t offset, SeekOrigin whence) override;
    int64_t tell() const override;
    int64_t size() const override;
    bool    eof() const override;
    bool    is_readable() const override;
    bool    is_writable() const override;
    bool    is_seekable() const override;

    // MemoryStream-specific API

    /// Create a shallow (COW) clone.  Both streams share the same
    /// underlying buffer until either one writes to it.
    MemoryStream clone() const;

    /// Read-only access to internal data.
    const std::vector<uint8_t>& get_data() const;

    /// Move the internal buffer out of the stream.  After this call
    /// the stream is empty.
    std::vector<uint8_t> release_data();

    /// Resize the backing store.  If the new size is smaller than the
    /// current position, the position is clamped.
    void resize(size_t new_size);

    /// Reserve capacity without changing the logical size.
    void reserve(size_t capacity);

    /// Reset the stream: clear all data, rewind to 0.
    void clear();

    /// Return a raw pointer to the beginning of the buffer.
    const uint8_t* raw_data() const;

    /// Current capacity of the underlying vector.
    size_t capacity() const;

private:
    /// Ensure this stream has its own unique copy of the data (break
    /// COW sharing).
    void detach();

    // Shared data block used for copy-on-write.
    struct SharedBuffer {
        std::vector<uint8_t> bytes;
        explicit SharedBuffer() = default;
        explicit SharedBuffer(const std::vector<uint8_t>& d) : bytes(d) {}
        explicit SharedBuffer(std::vector<uint8_t>&& d) : bytes(std::move(d)) {}
    };

    std::shared_ptr<SharedBuffer> m_buf;
    size_t     m_pos  = 0;
    StreamMode m_mode = StreamMode::ReadWrite;
};

} // namespace PixelForge
