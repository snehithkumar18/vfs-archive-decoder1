#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace PixelForge {

template <typename T>
constexpr bool checked_add(T left, T right, T& result) noexcept {
    static_assert(std::is_unsigned<T>::value, "checked_add requires an unsigned type");
    if (right > std::numeric_limits<T>::max() - left) return false;
    result = static_cast<T>(left + right);
    return true;
}

template <typename T>
constexpr bool checked_sub(T left, T right, T& result) noexcept {
    static_assert(std::is_unsigned<T>::value, "checked_sub requires an unsigned type");
    if (right > left) return false;
    result = static_cast<T>(left - right);
    return true;
}

template <typename T>
constexpr bool checked_mul(T left, T right, T& result) noexcept {
    static_assert(std::is_unsigned<T>::value, "checked_mul requires an unsigned type");
    if (left != 0 && right > std::numeric_limits<T>::max() / left) return false;
    result = static_cast<T>(left * right);
    return true;
}

template <typename T>
constexpr bool checked_align_up(T value, T alignment, T& result) noexcept {
    static_assert(std::is_unsigned<T>::value, "checked_align_up requires an unsigned type");
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return false;
    T adjusted = 0;
    if (!checked_add(value, static_cast<T>(alignment - 1), adjusted)) return false;
    result = static_cast<T>(adjusted & ~(alignment - 1));
    return true;
}

struct ImageLayout {
    size_t row_bytes{0};
    size_t pixel_bytes{0};
};

constexpr bool checked_image_layout(uint32_t width,
                                    uint32_t height,
                                    uint32_t channels,
                                    size_t row_alignment,
                                    ImageLayout& result) noexcept {
    if (width == 0 || height == 0 || channels == 0) return false;
    size_t row = 0;
    if (!checked_mul(static_cast<size_t>(width), static_cast<size_t>(channels), row)) return false;
    if (row_alignment > 1 && !checked_align_up(row, row_alignment, row)) return false;
    size_t total = 0;
    if (!checked_mul(row, static_cast<size_t>(height), total)) return false;
    result = {row, total};
    return true;
}

} // namespace PixelForge
