/**
 * @file serializer.cc
 * @brief Implementation of VFSSerializer – binary serialization engine.
 *
 * All write operations append to the tail of an internal std::vector<uint8_t>
 * buffer.  Multi-byte values are stored in the configured byte order
 * (little-endian or big-endian); when the target order differs from the
 * platform's native order the bytes are swapped before writing / after
 * reading.
 *
 * All read operations consume bytes starting at the current cursor position
 * and advance it.  A std::runtime_error is thrown if a read would exceed the
 * buffer boundary, preventing silent data corruption.
 *
 * Strings and byte-vectors are stored as length-prefixed blobs:
 *   [uint32_t length][raw payload bytes]
 *
 * The length field itself is subject to endianness conversion, ensuring that
 * archives produced on one platform can be loaded on another.
 *
 * Copyright (c) 2026 Project Fenrer contributors.
 */

#include "serializer.h"
#include "logger.h"

#include <cstring>
#include <stdexcept>
#include <sstream>

// ===================================================================
//  Detecting native byte order at compile / run time
// ===================================================================

/**
 * Determine the native byte order of the current platform.
 * We use a simple union trick rather than relying on macros so that this
 * works across compilers without configuration.
 */
static ByteOrder native_byte_order() {
    union {
        uint16_t value;
        uint8_t  bytes[2];
    } probe;
    probe.value = 0x0001;
    // If the first byte is 0x01 the platform stores the least-significant
    // byte first → little-endian.
    return (probe.bytes[0] == 0x01) ? ByteOrder::LittleEndian
                                    : ByteOrder::BigEndian;
}

// Cache the result so we only compute it once.
static const ByteOrder NATIVE_ORDER = native_byte_order();

// ===================================================================
//  Construction
// ===================================================================

VFSSerializer::VFSSerializer(ByteOrder order)
    : cursor_(0), order_(order) {
    VFSLogger::get_instance().debug(
        "VFSSerializer",
        std::string("Constructed empty serializer (") +
            (order == ByteOrder::LittleEndian ? "LE" : "BE") + ")");
}

VFSSerializer::VFSSerializer(ByteOrder order,
                             const std::vector<uint8_t>& data)
    : buffer_(data), cursor_(0), order_(order) {
    std::ostringstream msg;
    msg << "Constructed serializer from buffer ("
        << data.size() << " bytes, "
        << (order == ByteOrder::LittleEndian ? "LE" : "BE") << ")";
    VFSLogger::get_instance().debug("VFSSerializer", msg.str());
}

VFSSerializer::VFSSerializer(ByteOrder order,
                             const uint8_t* data, size_t length)
    : buffer_(data, data + length), cursor_(0), order_(order) {
    std::ostringstream msg;
    msg << "Constructed serializer from raw pointer ("
        << length << " bytes, "
        << (order == ByteOrder::LittleEndian ? "LE" : "BE") << ")";
    VFSLogger::get_instance().debug("VFSSerializer", msg.str());
}

// ===================================================================
//  Byte-swap helpers
// ===================================================================

bool VFSSerializer::needs_swap() const noexcept {
    return order_ != NATIVE_ORDER;
}

uint16_t VFSSerializer::swap16(uint16_t v) noexcept {
    return static_cast<uint16_t>((v >> 8) | (v << 8));
}

uint32_t VFSSerializer::swap32(uint32_t v) noexcept {
    return ((v & 0xFF000000u) >> 24) |
           ((v & 0x00FF0000u) >>  8) |
           ((v & 0x0000FF00u) <<  8) |
           ((v & 0x000000FFu) << 24);
}

uint64_t VFSSerializer::swap64(uint64_t v) noexcept {
    return ((v & 0xFF00000000000000ULL) >> 56) |
           ((v & 0x00FF000000000000ULL) >> 40) |
           ((v & 0x0000FF0000000000ULL) >> 24) |
           ((v & 0x000000FF00000000ULL) >>  8) |
           ((v & 0x00000000FF000000ULL) <<  8) |
           ((v & 0x0000000000FF0000ULL) << 24) |
           ((v & 0x000000000000FF00ULL) << 40) |
           ((v & 0x00000000000000FFULL) << 56);
}

// ===================================================================
//  Low-level buffer access
// ===================================================================

void VFSSerializer::append_raw(const void* src, size_t count) {
    const uint8_t* bytes = static_cast<const uint8_t*>(src);
    buffer_.insert(buffer_.end(), bytes, bytes + count);
}

void VFSSerializer::consume_raw(void* dst, size_t count) {
    if (cursor_ + count > buffer_.size()) {
        std::ostringstream err;
        err << "VFSSerializer::consume_raw: buffer overrun – attempted to read "
            << count << " bytes at offset " << cursor_
            << " but buffer size is " << buffer_.size();
        VFSLogger::get_instance().error("VFSSerializer", err.str());
        throw std::runtime_error(err.str());
    }
    std::memcpy(dst, buffer_.data() + cursor_, count);
    cursor_ += count;
}

// ===================================================================
//  Write operations
// ===================================================================

void VFSSerializer::write_uint8(uint8_t value) {
    buffer_.push_back(value);
}

void VFSSerializer::write_uint16(uint16_t value) {
    if (needs_swap()) value = swap16(value);
    append_raw(&value, sizeof(value));
}

void VFSSerializer::write_uint32(uint32_t value) {
    if (needs_swap()) value = swap32(value);
    append_raw(&value, sizeof(value));
}

void VFSSerializer::write_uint64(uint64_t value) {
    if (needs_swap()) value = swap64(value);
    append_raw(&value, sizeof(value));
}

void VFSSerializer::write_int32(int32_t value) {
    // Reinterpret the signed value as unsigned for byte-swapping purposes.
    // This preserves the bit pattern exactly.
    uint32_t uval;
    std::memcpy(&uval, &value, sizeof(uval));
    if (needs_swap()) uval = swap32(uval);
    append_raw(&uval, sizeof(uval));
}

void VFSSerializer::write_float(float value) {
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    // IEEE 754 float → treat as uint32 for byte-order conversion.
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if (needs_swap()) bits = swap32(bits);
    append_raw(&bits, sizeof(bits));
}

void VFSSerializer::write_double(double value) {
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if (needs_swap()) bits = swap64(bits);
    append_raw(&bits, sizeof(bits));
}

void VFSSerializer::write_string(const std::string& value) {
    // Guard against absurdly large strings that would overflow uint32.
    if (value.size() > 0xFFFFFFFFu) {
        VFSLogger::get_instance().error(
            "VFSSerializer",
            "write_string: string exceeds uint32 max length");
        throw std::runtime_error(
            "VFSSerializer::write_string: string too large");
    }

    // Length prefix (uint32).
    uint32_t len = static_cast<uint32_t>(value.size());
    write_uint32(len);

    // Raw character data – no null terminator.
    if (!value.empty()) {
        append_raw(value.data(), value.size());
    }

    VFSLogger::get_instance().debug(
        "VFSSerializer",
        "Wrote string (" + std::to_string(value.size()) + " bytes)");
}

void VFSSerializer::write_bytes(const std::vector<uint8_t>& value) {
    if (value.size() > 0xFFFFFFFFu) {
        VFSLogger::get_instance().error(
            "VFSSerializer",
            "write_bytes: payload exceeds uint32 max length");
        throw std::runtime_error(
            "VFSSerializer::write_bytes: payload too large");
    }

    uint32_t len = static_cast<uint32_t>(value.size());
    write_uint32(len);

    if (!value.empty()) {
        append_raw(value.data(), value.size());
    }

    VFSLogger::get_instance().debug(
        "VFSSerializer",
        "Wrote byte blob (" + std::to_string(value.size()) + " bytes)");
}

void VFSSerializer::write_bool(bool value) {
    buffer_.push_back(value ? 0x01 : 0x00);
}

// ===================================================================
//  Read operations
// ===================================================================

uint8_t VFSSerializer::read_uint8() {
    uint8_t v = 0;
    consume_raw(&v, sizeof(v));
    return v;
}

uint16_t VFSSerializer::read_uint16() {
    uint16_t v = 0;
    consume_raw(&v, sizeof(v));
    if (needs_swap()) v = swap16(v);
    return v;
}

uint32_t VFSSerializer::read_uint32() {
    uint32_t v = 0;
    consume_raw(&v, sizeof(v));
    if (needs_swap()) v = swap32(v);
    return v;
}

uint64_t VFSSerializer::read_uint64() {
    uint64_t v = 0;
    consume_raw(&v, sizeof(v));
    if (needs_swap()) v = swap64(v);
    return v;
}

int32_t VFSSerializer::read_int32() {
    uint32_t uval = 0;
    consume_raw(&uval, sizeof(uval));
    if (needs_swap()) uval = swap32(uval);
    int32_t result;
    std::memcpy(&result, &uval, sizeof(result));
    return result;
}

float VFSSerializer::read_float() {
    uint32_t bits = 0;
    consume_raw(&bits, sizeof(bits));
    if (needs_swap()) bits = swap32(bits);
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

double VFSSerializer::read_double() {
    uint64_t bits = 0;
    consume_raw(&bits, sizeof(bits));
    if (needs_swap()) bits = swap64(bits);
    double result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::string VFSSerializer::read_string() {
    // Read the uint32 length prefix.
    uint32_t len = read_uint32();

    // Sanity-check: the length must not exceed the remaining buffer.
    if (static_cast<size_t>(len) > remaining()) {
        std::ostringstream err;
        err << "VFSSerializer::read_string: declared length " << len
            << " exceeds remaining buffer (" << remaining() << " bytes)";
        VFSLogger::get_instance().error("VFSSerializer", err.str());
        throw std::runtime_error(err.str());
    }

    if (len == 0) return "";

    std::string result(len, '\0');
    consume_raw(&result[0], len);

    VFSLogger::get_instance().debug(
        "VFSSerializer",
        "Read string (" + std::to_string(len) + " bytes)");

    return result;
}

std::vector<uint8_t> VFSSerializer::read_bytes() {
    uint32_t len = read_uint32();

    if (static_cast<size_t>(len) > remaining()) {
        std::ostringstream err;
        err << "VFSSerializer::read_bytes: declared length " << len
            << " exceeds remaining buffer (" << remaining() << " bytes)";
        VFSLogger::get_instance().error("VFSSerializer", err.str());
        throw std::runtime_error(err.str());
    }

    if (len == 0) return {};

    std::vector<uint8_t> result(len);
    consume_raw(result.data(), len);

    VFSLogger::get_instance().debug(
        "VFSSerializer",
        "Read byte blob (" + std::to_string(len) + " bytes)");

    return result;
}

bool VFSSerializer::read_bool() {
    uint8_t v = read_uint8();
    return v != 0;
}

// ===================================================================
//  Buffer and cursor management
// ===================================================================

const std::vector<uint8_t>& VFSSerializer::get_buffer() const noexcept {
    return buffer_;
}

size_t VFSSerializer::get_size() const noexcept {
    return buffer_.size();
}

void VFSSerializer::reset() {
    buffer_.clear();
    cursor_ = 0;
    VFSLogger::get_instance().debug("VFSSerializer",
                                    "Buffer reset (cleared)");
}

void VFSSerializer::seek(size_t pos) {
    if (pos > buffer_.size()) {
        std::ostringstream err;
        err << "VFSSerializer::seek: position " << pos
            << " is beyond buffer end (" << buffer_.size() << ")";
        VFSLogger::get_instance().error("VFSSerializer", err.str());
        throw std::runtime_error(err.str());
    }
    cursor_ = pos;
}

size_t VFSSerializer::tell() const noexcept {
    return cursor_;
}

size_t VFSSerializer::remaining() const noexcept {
    if (cursor_ >= buffer_.size()) return 0;
    return buffer_.size() - cursor_;
}

ByteOrder VFSSerializer::get_byte_order() const noexcept {
    return order_;
}

void VFSSerializer::set_byte_order(ByteOrder order) noexcept {
    order_ = order;
    VFSLogger::get_instance().debug(
        "VFSSerializer",
        std::string("Byte order changed to ") +
            (order == ByteOrder::LittleEndian ? "LE" : "BE"));
}
