#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace PixelForge {

// ---------------------------------------------------------------
// Compile-time endianness detection
// ---------------------------------------------------------------

enum class Endianness {
    Little,
    Big
};

// Detect host byte order at compile time.  Covers MSVC, GCC/Clang.
inline constexpr Endianness host_endianness() {
#if defined(_MSC_VER)
    // MSVC targets are always little-endian (x86/x64/ARM LE)
    return Endianness::Little;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
    return (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) ? Endianness::Big
                                                    : Endianness::Little;
#else
    return Endianness::Little;  // safe default for modern hardware
#endif
}

inline constexpr bool is_little_endian() {
    return host_endianness() == Endianness::Little;
}

inline constexpr bool is_big_endian() {
    return host_endianness() == Endianness::Big;
}

// ---------------------------------------------------------------
// Low-level byte-swap primitives
// ---------------------------------------------------------------

inline constexpr uint16_t byte_swap_16(uint16_t v) noexcept {
    return static_cast<uint16_t>((v << 8) | (v >> 8));
}

inline constexpr uint32_t byte_swap_32(uint32_t v) noexcept {
    v = ((v & 0x00FF00FFu) << 8) | ((v & 0xFF00FF00u) >> 8);
    v = (v << 16) | (v >> 16);
    return v;
}

inline constexpr uint64_t byte_swap_64(uint64_t v) noexcept {
    v = ((v & 0x00FF00FF00FF00FFull) << 8)  | ((v & 0xFF00FF00FF00FF00ull) >> 8);
    v = ((v & 0x0000FFFF0000FFFFull) << 16) | ((v & 0xFFFF0000FFFF0000ull) >> 16);
    v = (v << 32) | (v >> 32);
    return v;
}

// ---------------------------------------------------------------
// Generic byte_swap dispatcher via SFINAE
// ---------------------------------------------------------------

template <typename T>
inline constexpr
typename std::enable_if<sizeof(T) == 1, T>::type
byte_swap(T v) noexcept {
    return v;  // single byte – nothing to swap
}

template <typename T>
inline
typename std::enable_if<sizeof(T) == 2, T>::type
byte_swap(T v) noexcept {
    uint16_t raw;
    std::memcpy(&raw, &v, 2);
    raw = byte_swap_16(raw);
    T result;
    std::memcpy(&result, &raw, 2);
    return result;
}

template <typename T>
inline
typename std::enable_if<sizeof(T) == 4, T>::type
byte_swap(T v) noexcept {
    uint32_t raw;
    std::memcpy(&raw, &v, 4);
    raw = byte_swap_32(raw);
    T result;
    std::memcpy(&result, &raw, 4);
    return result;
}

template <typename T>
inline
typename std::enable_if<sizeof(T) == 8, T>::type
byte_swap(T v) noexcept {
    uint64_t raw;
    std::memcpy(&raw, &v, 8);
    raw = byte_swap_64(raw);
    T result;
    std::memcpy(&result, &raw, 8);
    return result;
}

// ---------------------------------------------------------------
// Endian conversion functions
//
// to_little_endian / from_little_endian are identity on LE hosts.
// to_big_endian    / from_big_endian    are identity on BE hosts.
// ---------------------------------------------------------------

template <typename T>
inline T to_little_endian(T val) noexcept {
    if constexpr (is_little_endian()) {
        return val;
    } else {
        return byte_swap<T>(val);
    }
}

template <typename T>
inline T from_little_endian(T val) noexcept {
    // Symmetric: the same transformation converts both ways
    return to_little_endian<T>(val);
}

template <typename T>
inline T to_big_endian(T val) noexcept {
    if constexpr (is_big_endian()) {
        return val;
    } else {
        return byte_swap<T>(val);
    }
}

template <typename T>
inline T from_big_endian(T val) noexcept {
    return to_big_endian<T>(val);
}

// ---------------------------------------------------------------
// Byte-level pack/unpack (always safe, avoids alignment issues)
// ---------------------------------------------------------------

inline uint16_t pack_u16_le(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

inline uint16_t pack_u16_be(const uint8_t* p) noexcept {
    return (static_cast<uint16_t>(p[0]) << 8) |
            static_cast<uint16_t>(p[1]);
}

inline uint32_t pack_u32_le(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0])        |
          (static_cast<uint32_t>(p[1]) << 8)  |
          (static_cast<uint32_t>(p[2]) << 16) |
          (static_cast<uint32_t>(p[3]) << 24);
}

inline uint32_t pack_u32_be(const uint8_t* p) noexcept {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

inline uint64_t pack_u64_le(const uint8_t* p) noexcept {
    return static_cast<uint64_t>(p[0])        |
          (static_cast<uint64_t>(p[1]) << 8)  |
          (static_cast<uint64_t>(p[2]) << 16) |
          (static_cast<uint64_t>(p[3]) << 24) |
          (static_cast<uint64_t>(p[4]) << 32) |
          (static_cast<uint64_t>(p[5]) << 40) |
          (static_cast<uint64_t>(p[6]) << 48) |
          (static_cast<uint64_t>(p[7]) << 56);
}

inline uint64_t pack_u64_be(const uint8_t* p) noexcept {
    return (static_cast<uint64_t>(p[0]) << 56) |
           (static_cast<uint64_t>(p[1]) << 48) |
           (static_cast<uint64_t>(p[2]) << 40) |
           (static_cast<uint64_t>(p[3]) << 32) |
           (static_cast<uint64_t>(p[4]) << 24) |
           (static_cast<uint64_t>(p[5]) << 16) |
           (static_cast<uint64_t>(p[6]) << 8)  |
            static_cast<uint64_t>(p[7]);
}

inline void unpack_u16_le(uint16_t v, uint8_t* p) noexcept {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}

inline void unpack_u16_be(uint16_t v, uint8_t* p) noexcept {
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
}

inline void unpack_u32_le(uint32_t v, uint8_t* p) noexcept {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

inline void unpack_u32_be(uint32_t v, uint8_t* p) noexcept {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v);
}

inline void unpack_u64_le(uint64_t v, uint8_t* p) noexcept {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<uint8_t>(v >> (i * 8));
    }
}

inline void unpack_u64_be(uint64_t v, uint8_t* p) noexcept {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<uint8_t>(v >> ((7 - i) * 8));
    }
}

} // namespace PixelForge
