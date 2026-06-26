/**
 * @file serializer.h
 * @brief Binary serialization / deserialization engine for FenrerVFS.
 *
 * VFSSerializer provides a buffer-backed, cursor-driven facility for
 * writing and reading typed binary data.  It supports both little-endian
 * (LE) and big-endian (BE) byte ordering so that archive blobs can be
 * produced and consumed in a platform-independent manner.
 *
 * Write operations always append to the end of the internal buffer.
 * Read operations consume data at the current read-cursor position and
 * advance it by the number of bytes read.  Every read is bounds-checked
 * and will throw std::runtime_error on buffer overrun.
 *
 * Strings are serialised as length-prefixed blobs: a uint32 byte count
 * followed by the raw character data (no null terminator stored).
 *
 * Usage example:
 * @code
 *   VFSSerializer s(ByteOrder::LittleEndian);
 *   s.write_uint32(0xDEADBEEF);
 *   s.write_string("hello");
 *   s.write_bool(true);
 *
 *   // ... persist s.get_buffer() somewhere ...
 *
 *   // Later, to read back:
 *   VFSSerializer reader(ByteOrder::LittleEndian, saved_buffer);
 *   uint32_t magic  = reader.read_uint32();
 *   std::string msg = reader.read_string();
 *   bool flag       = reader.read_bool();
 * @endcode
 *
 * Copyright (c) 2026 Project Fenrer contributors.
 */

#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// -----------------------------------------------------------------------
//  Byte-order enumeration
// -----------------------------------------------------------------------

enum class ByteOrder {
    LittleEndian,
    BigEndian
};

// -----------------------------------------------------------------------
//  VFSSerializer
// -----------------------------------------------------------------------

class VFSSerializer {
public:
    // -------------------------------------------------------------------
    //  Construction
    // -------------------------------------------------------------------

    /**
     * Construct an empty serializer with the given byte order.
     * The buffer starts empty and the read cursor at position 0.
     */
    explicit VFSSerializer(ByteOrder order = ByteOrder::LittleEndian);

    /**
     * Construct a serializer pre-loaded with @p data.
     * Useful for deserialization: the read cursor starts at 0.
     */
    VFSSerializer(ByteOrder order, const std::vector<uint8_t>& data);

    /**
     * Construct a serializer pre-loaded from a raw pointer + length.
     */
    VFSSerializer(ByteOrder order, const uint8_t* data, size_t length);

    ~VFSSerializer() = default;

    // Copyable and movable.
    VFSSerializer(const VFSSerializer&) = default;
    VFSSerializer& operator=(const VFSSerializer&) = default;
    VFSSerializer(VFSSerializer&&) noexcept = default;
    VFSSerializer& operator=(VFSSerializer&&) noexcept = default;

    // -------------------------------------------------------------------
    //  Write (append) operations
    // -------------------------------------------------------------------

    void write_uint8(uint8_t value);
    void write_uint16(uint16_t value);
    void write_uint32(uint32_t value);
    void write_uint64(uint64_t value);
    void write_int32(int32_t value);
    void write_float(float value);
    void write_double(double value);

    /**
     * Write a length-prefixed string.
     * Layout: [uint32 byte_count][raw bytes]   (no null terminator)
     */
    void write_string(const std::string& value);

    /**
     * Write a raw byte sequence prefixed with its uint32 length.
     */
    void write_bytes(const std::vector<uint8_t>& value);

    /**
     * Write a boolean as a single byte (0x00 = false, 0x01 = true).
     */
    void write_bool(bool value);

    // -------------------------------------------------------------------
    //  Read (cursor-driven) operations
    // -------------------------------------------------------------------

    uint8_t   read_uint8();
    uint16_t  read_uint16();
    uint32_t  read_uint32();
    uint64_t  read_uint64();
    int32_t   read_int32();
    float     read_float();
    double    read_double();

    /**
     * Read a length-prefixed string written by write_string().
     */
    std::string read_string();

    /**
     * Read a length-prefixed byte sequence written by write_bytes().
     */
    std::vector<uint8_t> read_bytes();

    /**
     * Read a single byte and return true if non-zero.
     */
    bool read_bool();

    // -------------------------------------------------------------------
    //  Buffer and cursor management
    // -------------------------------------------------------------------

    /** Return a const reference to the internal buffer. */
    const std::vector<uint8_t>& get_buffer() const noexcept;

    /** Return the current size of the buffer in bytes. */
    size_t get_size() const noexcept;

    /** Clear the buffer and reset the read cursor to 0. */
    void reset();

    /**
     * Set the read cursor to an absolute byte position.
     * @throws std::runtime_error if @p pos > buffer size.
     */
    void seek(size_t pos);

    /** Return the current read-cursor position. */
    size_t tell() const noexcept;

    /** Return the number of bytes remaining after the cursor. */
    size_t remaining() const noexcept;

    /** Return the configured byte order. */
    ByteOrder get_byte_order() const noexcept;

    /** Change the byte order for subsequent reads/writes. */
    void set_byte_order(ByteOrder order) noexcept;

private:
    // -------------------------------------------------------------------
    //  Internal helpers
    // -------------------------------------------------------------------

    /** Append @p count raw bytes from @p src to the buffer. */
    void append_raw(const void* src, size_t count);

    /**
     * Read @p count bytes from the buffer at the current cursor into @p dst
     * and advance the cursor.
     * @throws std::runtime_error on overrun.
     */
    void consume_raw(void* dst, size_t count);

    /**
     * Return true when a byte-swap is needed (i.e. the configured byte
     * order differs from the native byte order of this platform).
     */
    bool needs_swap() const noexcept;

    /** Byte-swap a 16-bit value. */
    static uint16_t swap16(uint16_t v) noexcept;

    /** Byte-swap a 32-bit value. */
    static uint32_t swap32(uint32_t v) noexcept;

    /** Byte-swap a 64-bit value. */
    static uint64_t swap64(uint64_t v) noexcept;

    // -------------------------------------------------------------------
    //  Data members
    // -------------------------------------------------------------------

    std::vector<uint8_t> buffer_;   ///< Internal byte buffer.
    size_t               cursor_;   ///< Current read position.
    ByteOrder            order_;    ///< Configured byte order.
};

#endif // SERIALIZER_H
