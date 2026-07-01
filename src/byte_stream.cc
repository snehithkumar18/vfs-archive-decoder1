#include "byte_stream.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>

namespace PixelForge {

bool ByteView::contains(size_t offset, size_t count) const noexcept {
    return offset <= size_ && count <= size_ - offset && (count == 0 || data_ != nullptr);
}

bool ByteView::subview(size_t offset, size_t count, ByteView& output) const noexcept {
    if (!contains(offset, count)) return false;
    output = ByteView(data_ ? data_ + offset : nullptr, count);
    return true;
}

ByteReader::ByteReader(ByteView input, ByteOrder order) noexcept : input_(input), order_(order) {}

ByteReader::ByteReader(const uint8_t* data, size_t size, ByteOrder order) noexcept
    : ByteReader(ByteView(data, size), order) {}

ByteReader::ByteReader(const std::vector<uint8_t>& bytes, ByteOrder order) noexcept
    : ByteReader(ByteView(bytes), order) {}

size_t ByteReader::remaining() const noexcept {
    return position_ <= input_.size() ? input_.size() - position_ : 0;
}

bool ByteReader::seek(size_t absolute_position) noexcept {
    if (absolute_position > input_.size()) return false;
    position_ = absolute_position;
    return true;
}

bool ByteReader::skip(size_t count) noexcept {
    size_t next = 0;
    if (!checked_add(position_, count, next) || next > input_.size()) return false;
    position_ = next;
    return true;
}

bool ByteReader::align(size_t alignment) noexcept {
    size_t next = 0;
    if (!checked_align_up(position_, alignment, next) || next > input_.size()) return false;
    position_ = next;
    return true;
}

bool ByteReader::peek_u8(uint8_t& value) const noexcept {
    if (!input_.contains(position_)) return false;
    value = input_[position_];
    return true;
}

bool ByteReader::read_u8(uint8_t& value) noexcept {
    if (!peek_u8(value)) return false;
    ++position_;
    return true;
}

bool ByteReader::read_i8(int8_t& value) noexcept {
    uint8_t raw = 0;
    if (!read_u8(raw)) return false;
    std::memcpy(&value, &raw, sizeof(value));
    return true;
}

template <typename T>
bool ByteReader::read_integer(T& value) noexcept {
    using Unsigned = typename std::make_unsigned<T>::type;
    constexpr size_t width = sizeof(T);
    if (!input_.contains(position_, width)) return false;
    Unsigned result = 0;
    if (order_ == ByteOrder::LittleEndian) {
        for (size_t i = 0; i < width; ++i) {
            result |= static_cast<Unsigned>(input_[position_ + i]) << (i * 8);
        }
    } else {
        for (size_t i = 0; i < width; ++i) {
            result = static_cast<Unsigned>((result << 8) | input_[position_ + i]);
        }
    }
    position_ += width;
    std::memcpy(&value, &result, sizeof(value));
    return true;
}

bool ByteReader::read_u16(uint16_t& value) noexcept { return read_integer(value); }
bool ByteReader::read_i16(int16_t& value) noexcept { return read_integer(value); }

bool ByteReader::read_u24(uint32_t& value) noexcept {
    if (!input_.contains(position_, 3)) return false;
    if (order_ == ByteOrder::LittleEndian) {
        value = static_cast<uint32_t>(input_[position_]) |
                (static_cast<uint32_t>(input_[position_ + 1]) << 8) |
                (static_cast<uint32_t>(input_[position_ + 2]) << 16);
    } else {
        value = (static_cast<uint32_t>(input_[position_]) << 16) |
                (static_cast<uint32_t>(input_[position_ + 1]) << 8) |
                static_cast<uint32_t>(input_[position_ + 2]);
    }
    position_ += 3;
    return true;
}

bool ByteReader::read_u32(uint32_t& value) noexcept { return read_integer(value); }
bool ByteReader::read_i32(int32_t& value) noexcept { return read_integer(value); }
bool ByteReader::read_u64(uint64_t& value) noexcept { return read_integer(value); }
bool ByteReader::read_i64(int64_t& value) noexcept { return read_integer(value); }

bool ByteReader::read_bytes(uint8_t* destination, size_t count) noexcept {
    if ((count != 0 && destination == nullptr) || !input_.contains(position_, count)) return false;
    if (count != 0) std::memcpy(destination, input_.data() + position_, count);
    position_ += count;
    return true;
}

bool ByteReader::read_view(size_t count, ByteView& output) noexcept {
    if (!input_.subview(position_, count, output)) return false;
    position_ += count;
    return true;
}

bool ByteReader::read_vector(size_t count, std::vector<uint8_t>& output) {
    if (!input_.contains(position_, count)) return false;
    try {
        output.assign(input_.data() + position_, input_.data() + position_ + count);
    } catch (const std::bad_alloc&) {
        return false;
    }
    position_ += count;
    return true;
}

bool ByteReader::read_fixed_string(size_t count, std::string& output) {
    if (!input_.contains(position_, count)) return false;
    try {
        output.assign(reinterpret_cast<const char*>(input_.data() + position_), count);
    } catch (const std::bad_alloc&) {
        return false;
    }
    position_ += count;
    return true;
}

bool ByteReader::read_c_string(size_t maximum_size, std::string& output) {
    const size_t available = std::min(maximum_size, remaining());
    size_t length = 0;
    while (length < available && input_[position_ + length] != 0) ++length;
    if (length == available) return false;
    try {
        output.assign(reinterpret_cast<const char*>(input_.data() + position_), length);
    } catch (const std::bad_alloc&) {
        return false;
    }
    position_ += length + 1;
    return true;
}

bool ByteWriter::reserve(size_t capacity) {
    try {
        bytes_.reserve(capacity);
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

bool ByteWriter::align(size_t alignment, uint8_t fill) {
    size_t aligned = 0;
    if (!checked_align_up(bytes_.size(), alignment, aligned)) return false;
    try {
        bytes_.resize(aligned, fill);
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

template <typename T>
bool ByteWriter::write_integer(T value) {
    using Unsigned = typename std::make_unsigned<T>::type;
    Unsigned raw = 0;
    std::memcpy(&raw, &value, sizeof(value));
    try {
        if (order_ == ByteOrder::LittleEndian) {
            for (size_t i = 0; i < sizeof(T); ++i) bytes_.push_back(static_cast<uint8_t>(raw >> (i * 8)));
        } else {
            for (size_t i = 0; i < sizeof(T); ++i) {
                const size_t shift = (sizeof(T) - i - 1) * 8;
                bytes_.push_back(static_cast<uint8_t>(raw >> shift));
            }
        }
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

bool ByteWriter::write_u8(uint8_t value) { return write_integer(value); }
bool ByteWriter::write_i8(int8_t value) { return write_integer(value); }
bool ByteWriter::write_u16(uint16_t value) { return write_integer(value); }
bool ByteWriter::write_i16(int16_t value) { return write_integer(value); }

bool ByteWriter::write_u24(uint32_t value) {
    if (value > 0xFFFFFFu) return false;
    try {
        if (order_ == ByteOrder::LittleEndian) {
            bytes_.push_back(static_cast<uint8_t>(value));
            bytes_.push_back(static_cast<uint8_t>(value >> 8));
            bytes_.push_back(static_cast<uint8_t>(value >> 16));
        } else {
            bytes_.push_back(static_cast<uint8_t>(value >> 16));
            bytes_.push_back(static_cast<uint8_t>(value >> 8));
            bytes_.push_back(static_cast<uint8_t>(value));
        }
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

bool ByteWriter::write_u32(uint32_t value) { return write_integer(value); }
bool ByteWriter::write_i32(int32_t value) { return write_integer(value); }
bool ByteWriter::write_u64(uint64_t value) { return write_integer(value); }
bool ByteWriter::write_i64(int64_t value) { return write_integer(value); }

bool ByteWriter::write_bytes(ByteView bytes) { return write_bytes(bytes.data(), bytes.size()); }

bool ByteWriter::write_bytes(const uint8_t* bytes, size_t count) {
    if (count != 0 && bytes == nullptr) return false;
    try {
        bytes_.insert(bytes_.end(), bytes, bytes + count);
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

bool ByteWriter::write_string(const std::string& value, bool nul_terminate) {
    if (!write_bytes(reinterpret_cast<const uint8_t*>(value.data()), value.size())) return false;
    return !nul_terminate || write_u8(0);
}

template <typename T>
bool ByteWriter::patch_integer(size_t offset, T value) noexcept {
    if (offset > bytes_.size() || sizeof(T) > bytes_.size() - offset) return false;
    using Unsigned = typename std::make_unsigned<T>::type;
    Unsigned raw = 0;
    std::memcpy(&raw, &value, sizeof(value));
    for (size_t i = 0; i < sizeof(T); ++i) {
        const size_t shift = order_ == ByteOrder::LittleEndian ? i * 8 : (sizeof(T) - i - 1) * 8;
        bytes_[offset + i] = static_cast<uint8_t>(raw >> shift);
    }
    return true;
}

bool ByteWriter::patch_u16(size_t offset, uint16_t value) noexcept { return patch_integer(offset, value); }
bool ByteWriter::patch_u32(size_t offset, uint32_t value) noexcept { return patch_integer(offset, value); }
bool ByteWriter::patch_u64(size_t offset, uint64_t value) noexcept { return patch_integer(offset, value); }

} // namespace PixelForge
