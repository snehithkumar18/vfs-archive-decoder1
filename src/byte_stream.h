#pragma once

#include "checked_math.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PixelForge {

enum class ByteOrder {
    LittleEndian,
    BigEndian
};

class ByteView {
public:
    constexpr ByteView() noexcept = default;
    constexpr ByteView(const uint8_t* data, size_t size) noexcept : data_(data), size_(size) {}
    explicit ByteView(const std::vector<uint8_t>& bytes) noexcept : ByteView(bytes.data(), bytes.size()) {}

    constexpr const uint8_t* data() const noexcept { return data_; }
    constexpr size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    bool contains(size_t offset, size_t count = 1) const noexcept;
    bool subview(size_t offset, size_t count, ByteView& output) const noexcept;
    uint8_t operator[](size_t index) const noexcept { return data_[index]; }

private:
    const uint8_t* data_{nullptr};
    size_t size_{0};
};

class ByteReader {
public:
    explicit ByteReader(ByteView input, ByteOrder order = ByteOrder::LittleEndian) noexcept;
    ByteReader(const uint8_t* data, size_t size, ByteOrder order = ByteOrder::LittleEndian) noexcept;
    explicit ByteReader(const std::vector<uint8_t>& bytes, ByteOrder order = ByteOrder::LittleEndian) noexcept;

    size_t position() const noexcept { return position_; }
    size_t size() const noexcept { return input_.size(); }
    size_t remaining() const noexcept;
    bool eof() const noexcept { return remaining() == 0; }
    ByteOrder byte_order() const noexcept { return order_; }

    bool seek(size_t absolute_position) noexcept;
    bool skip(size_t count) noexcept;
    bool align(size_t alignment) noexcept;

    bool peek_u8(uint8_t& value) const noexcept;
    bool read_u8(uint8_t& value) noexcept;
    bool read_i8(int8_t& value) noexcept;
    bool read_u16(uint16_t& value) noexcept;
    bool read_i16(int16_t& value) noexcept;
    bool read_u24(uint32_t& value) noexcept;
    bool read_u32(uint32_t& value) noexcept;
    bool read_i32(int32_t& value) noexcept;
    bool read_u64(uint64_t& value) noexcept;
    bool read_i64(int64_t& value) noexcept;

    bool read_bytes(uint8_t* destination, size_t count) noexcept;
    bool read_view(size_t count, ByteView& output) noexcept;
    bool read_vector(size_t count, std::vector<uint8_t>& output);
    bool read_fixed_string(size_t count, std::string& output);
    bool read_c_string(size_t maximum_size, std::string& output);

private:
    template <typename T>
    bool read_integer(T& value) noexcept;

    ByteView input_;
    size_t position_{0};
    ByteOrder order_{ByteOrder::LittleEndian};
};

class ByteWriter {
public:
    explicit ByteWriter(ByteOrder order = ByteOrder::LittleEndian) : order_(order) {}
    explicit ByteWriter(std::vector<uint8_t> initial, ByteOrder order = ByteOrder::LittleEndian)
        : bytes_(std::move(initial)), order_(order) {}

    size_t position() const noexcept { return bytes_.size(); }
    ByteOrder byte_order() const noexcept { return order_; }
    const std::vector<uint8_t>& data() const noexcept { return bytes_; }
    std::vector<uint8_t> take() noexcept { return std::move(bytes_); }
    void clear() noexcept { bytes_.clear(); }

    bool reserve(size_t capacity);
    bool align(size_t alignment, uint8_t fill = 0);
    bool write_u8(uint8_t value);
    bool write_i8(int8_t value);
    bool write_u16(uint16_t value);
    bool write_i16(int16_t value);
    bool write_u24(uint32_t value);
    bool write_u32(uint32_t value);
    bool write_i32(int32_t value);
    bool write_u64(uint64_t value);
    bool write_i64(int64_t value);
    bool write_bytes(ByteView bytes);
    bool write_bytes(const uint8_t* bytes, size_t count);
    bool write_string(const std::string& value, bool nul_terminate = false);
    bool patch_u16(size_t offset, uint16_t value) noexcept;
    bool patch_u32(size_t offset, uint32_t value) noexcept;
    bool patch_u64(size_t offset, uint64_t value) noexcept;

private:
    template <typename T>
    bool write_integer(T value);
    template <typename T>
    bool patch_integer(size_t offset, T value) noexcept;

    std::vector<uint8_t> bytes_;
    ByteOrder order_{ByteOrder::LittleEndian};
};

} // namespace PixelForge
