#pragma once

#include "errors.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

namespace PixelForge {

// Seek origin for stream positioning
enum class SeekOrigin {
    Begin,    // Seek from the beginning of the stream
    Current,  // Seek relative to the current position
    End       // Seek from the end of the stream
};

// Mode used when opening or constructing a stream
enum class StreamMode {
    Read,
    Write,
    ReadWrite
};

// Result type for stream operations that may fail
struct StreamResult {
    size_t bytes_transferred = 0;
    PixelForgeErrorCode error = PixelForgeErrorCode::SUCCESS;

    bool ok() const { return error == PixelForgeErrorCode::SUCCESS; }
    explicit operator bool() const { return ok(); }
};

/// Abstract base class for byte-oriented I/O streams.
///
/// Concrete subclasses (MemoryStream, FileStream, etc.) implement the
/// pure virtual methods.  The base class provides convenience helpers
/// for reading/writing fixed-width integers in specific byte orders
/// and for reading plain-old-data structs.
class Stream {
public:
    virtual ~Stream() = default;

    // -----------------------------------------------------------------
    // Pure virtual interface – every subclass must implement these
    // -----------------------------------------------------------------

    /// Read up to `size` bytes into `buf`.
    /// Returns the number of bytes actually read, or 0 on error / EOF.
    virtual size_t read(void* buf, size_t size) = 0;

    /// Write `size` bytes from `buf`.
    /// Returns the number of bytes actually written.
    virtual size_t write(const void* buf, size_t size) = 0;

    /// Seek to `offset` relative to `whence`.
    /// Returns true on success.
    virtual bool seek(int64_t offset, SeekOrigin whence) = 0;

    /// Return the current read/write position.
    virtual int64_t tell() const = 0;

    /// Return the total size of the underlying data, or -1 if unknown.
    virtual int64_t size() const = 0;

    /// True when the stream has reached end-of-file.
    virtual bool eof() const = 0;

    /// Capability queries.
    virtual bool is_readable() const = 0;
    virtual bool is_writable() const = 0;
    virtual bool is_seekable() const = 0;

    // -----------------------------------------------------------------
    // Convenience read helpers (implemented in stream.cc)
    // -----------------------------------------------------------------

    /// Read a single unsigned byte.  Returns false on failure.
    bool read_u8(uint8_t& out);

    /// Read a 16-bit unsigned integer in little-endian byte order.
    bool read_u16_le(uint16_t& out);

    /// Read a 16-bit unsigned integer in big-endian byte order.
    bool read_u16_be(uint16_t& out);

    /// Read a 32-bit unsigned integer in little-endian byte order.
    bool read_u32_le(uint32_t& out);

    /// Read a 32-bit unsigned integer in big-endian byte order.
    bool read_u32_be(uint32_t& out);

    /// Read a 32-bit signed integer in little-endian byte order.
    bool read_i32_le(int32_t& out);

    /// Read a 32-bit signed integer in big-endian byte order.
    bool read_i32_be(int32_t& out);

    /// Read a 64-bit unsigned integer in little-endian byte order.
    bool read_u64_le(uint64_t& out);

    /// Read a 64-bit unsigned integer in big-endian byte order.
    bool read_u64_be(uint64_t& out);

    /// Read exactly `count` bytes.  Returns false if fewer are available.
    bool read_exact(void* buf, size_t count);

    /// Skip `count` bytes forward. Returns false if the stream cannot
    /// advance that far.
    bool skip(size_t count);

    // -----------------------------------------------------------------
    // Convenience write helpers (implemented in stream.cc)
    // -----------------------------------------------------------------

    bool write_u8(uint8_t val);
    bool write_u16_le(uint16_t val);
    bool write_u16_be(uint16_t val);
    bool write_u32_le(uint32_t val);
    bool write_u32_be(uint32_t val);
    bool write_i32_le(int32_t val);
    bool write_i32_be(int32_t val);
    bool write_u64_le(uint64_t val);
    bool write_u64_be(uint64_t val);

    /// Write all bytes from `buf`.  Returns false if not all could be
    /// written.
    bool write_exact(const void* buf, size_t count);

    // -----------------------------------------------------------------
    // Struct / POD read/write (header-only)
    // -----------------------------------------------------------------

    /// Read a POD struct of type T from the stream.  The struct is read
    /// in its in-memory representation (no byte-swapping).
    /// Returns true on success.
    template <typename T>
    bool read_struct(T& out) {
        static_assert(std::is_trivially_copyable<T>::value,
                      "read_struct<T> requires a trivially copyable type");
        return read_exact(&out, sizeof(T));
    }

    /// Write a POD struct to the stream.
    template <typename T>
    bool write_struct(const T& val) {
        static_assert(std::is_trivially_copyable<T>::value,
                      "write_struct<T> requires a trivially copyable type");
        return write_exact(&val, sizeof(T));
    }

    /// Read a string of `length` bytes from the stream.
    bool read_string(std::string& out, size_t length);

    /// Write a string (without null terminator unless it is part of the
    /// string data).
    bool write_string(const std::string& str);

    /// Rewind to the beginning of the stream.
    bool rewind() { return seek(0, SeekOrigin::Begin); }

    /// Return the number of bytes remaining (size - tell), or -1 if the
    /// size is unknown.
    int64_t remaining() const {
        int64_t s = size();
        if (s < 0) return -1;
        int64_t t = tell();
        if (t < 0) return -1;
        return (s > t) ? (s - t) : 0;
    }

protected:
    Stream() = default;
    // Non-copyable
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    // Movable
    Stream(Stream&&) = default;
    Stream& operator=(Stream&&) = default;
};

} // namespace PixelForge
