#pragma once

#include "errors.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace PixelForge {

// =================================================================
// Core checked arithmetic – works with both signed and unsigned types
// =================================================================

// ---- checked_add ------------------------------------------------

/// Checked addition for unsigned types.
/// Returns true on success, false on overflow.
template <typename T>
constexpr
typename std::enable_if<std::is_unsigned<T>::value, bool>::type
checked_add(T a, T b, T& result) noexcept {
    if (b > std::numeric_limits<T>::max() - a) return false;
    result = static_cast<T>(a + b);
    return true;
}

/// Checked addition for signed types.
/// Detects both positive and negative overflow.
template <typename T>
constexpr
typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value, bool>::type
checked_add(T a, T b, T& result) noexcept {
    // Positive overflow: a > 0 && b > max - a
    if (b > 0 && a > std::numeric_limits<T>::max() - b) return false;
    // Negative overflow: b < 0 && a < min - b
    if (b < 0 && a < std::numeric_limits<T>::min() - b) return false;
    result = static_cast<T>(a + b);
    return true;
}

// ---- checked_sub ------------------------------------------------

/// Checked subtraction for unsigned types.
template <typename T>
constexpr
typename std::enable_if<std::is_unsigned<T>::value, bool>::type
checked_sub(T a, T b, T& result) noexcept {
    if (b > a) return false;
    result = static_cast<T>(a - b);
    return true;
}

/// Checked subtraction for signed types.
template <typename T>
constexpr
typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value, bool>::type
checked_sub(T a, T b, T& result) noexcept {
    // a - b == a + (-b); overflow when b is negative and subtraction
    // pushes past max, or when b is positive and pushes past min.
    if (b > 0 && a < std::numeric_limits<T>::min() + b) return false;
    if (b < 0 && a > std::numeric_limits<T>::max() + b) return false;
    result = static_cast<T>(a - b);
    return true;
}

// ---- checked_mul ------------------------------------------------

/// Checked multiplication for unsigned types.
template <typename T>
constexpr
typename std::enable_if<std::is_unsigned<T>::value, bool>::type
checked_mul(T a, T b, T& result) noexcept {
    if (a != 0 && b > std::numeric_limits<T>::max() / a) return false;
    result = static_cast<T>(a * b);
    return true;
}

/// Checked multiplication for signed types.
/// Handles all four sign combinations.
template <typename T>
constexpr
typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value, bool>::type
checked_mul(T a, T b, T& result) noexcept {
    if (a == 0 || b == 0) { result = 0; return true; }

    constexpr T tmin = std::numeric_limits<T>::min();
    constexpr T tmax = std::numeric_limits<T>::max();

    if (a > 0) {
        if (b > 0) {
            if (a > tmax / b) return false;
        } else {
            // b < 0
            if (b < tmin / a) return false;
        }
    } else {
        // a < 0
        if (b > 0) {
            if (a < tmin / b) return false;
        } else {
            // both negative
            if (a != 0 && b < tmax / a) return false;
        }
    }
    result = static_cast<T>(a * b);
    return true;
}

// ---- checked_div ------------------------------------------------

/// Checked division.  Catches divide-by-zero and signed MIN / -1 overflow.
template <typename T>
constexpr
typename std::enable_if<std::is_integral<T>::value, bool>::type
checked_div(T a, T b, T& result) noexcept {
    if (b == 0) return false;
    if constexpr (std::is_signed<T>::value) {
        // INT_MIN / -1 overflows
        if (a == std::numeric_limits<T>::min() && b == static_cast<T>(-1))
            return false;
    }
    result = static_cast<T>(a / b);
    return true;
}

// =================================================================
// safe_cast – convert between integer types with overflow checking
// =================================================================

/// Cast `from` to type `To`, returning false if the value cannot be
/// represented exactly in the target type.
template <typename To, typename From>
constexpr
typename std::enable_if<std::is_integral<To>::value && std::is_integral<From>::value, bool>::type
safe_cast(From from, To& result) noexcept {
    // Both unsigned
    if constexpr (std::is_unsigned<From>::value && std::is_unsigned<To>::value) {
        if (from > std::numeric_limits<To>::max()) return false;
        result = static_cast<To>(from);
        return true;
    }
    // Both signed
    else if constexpr (std::is_signed<From>::value && std::is_signed<To>::value) {
        if (from < std::numeric_limits<To>::min() ||
            from > std::numeric_limits<To>::max())
            return false;
        result = static_cast<To>(from);
        return true;
    }
    // From signed, To unsigned
    else if constexpr (std::is_signed<From>::value && std::is_unsigned<To>::value) {
        if (from < 0) return false;
        auto ufrom = static_cast<typename std::make_unsigned<From>::type>(from);
        if (ufrom > std::numeric_limits<To>::max()) return false;
        result = static_cast<To>(ufrom);
        return true;
    }
    // From unsigned, To signed
    else {
        if (from > static_cast<typename std::make_unsigned<To>::type>(
                        std::numeric_limits<To>::max()))
            return false;
        result = static_cast<To>(from);
        return true;
    }
}

// =================================================================
// Image allocation safety
// =================================================================

/// Check whether width * height * channels fits in a size_t.
/// If it does, store the result in `out` and return true.
inline bool checked_alloc_size(uint32_t width, uint32_t height,
                               uint32_t channels, size_t& out) noexcept {
    size_t w = static_cast<size_t>(width);
    size_t h = static_cast<size_t>(height);
    size_t c = static_cast<size_t>(channels);
    size_t wc = 0;
    if (!checked_mul(w, c, wc)) return false;
    if (!checked_mul(wc, h, out)) return false;
    return true;
}

/// Overload that also validates non-zero dimensions.
inline bool checked_alloc_size_safe(uint32_t width, uint32_t height,
                                    uint32_t channels, size_t& out) noexcept {
    if (width == 0 || height == 0 || channels == 0) return false;
    return checked_alloc_size(width, height, channels, out);
}

// =================================================================
// Checked alignment (from original checked_math.h)
// =================================================================

template <typename T>
constexpr
typename std::enable_if<std::is_unsigned<T>::value, bool>::type
checked_align_up(T value, T alignment, T& result) noexcept {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return false;
    T adjusted = 0;
    if (!checked_add(value, static_cast<T>(alignment - 1), adjusted)) return false;
    result = static_cast<T>(adjusted & ~(alignment - 1));
    return true;
}

// =================================================================
// ImageLayout (from original checked_math.h)
// =================================================================

struct ImageLayout {
    size_t row_bytes{0};
    size_t pixel_bytes{0};
};

constexpr bool checked_image_layout(uint32_t width, uint32_t height,
                                    uint32_t channels,
                                    size_t row_alignment,
                                    ImageLayout& result) noexcept {
    if (width == 0 || height == 0 || channels == 0) return false;
    size_t row = 0;
    if (!checked_mul(static_cast<size_t>(width),
                     static_cast<size_t>(channels), row))
        return false;
    if (row_alignment > 1 &&
        !checked_align_up(row, row_alignment, row))
        return false;
    size_t total = 0;
    if (!checked_mul(row, static_cast<size_t>(height), total)) return false;
    result = {row, total};
    return true;
}

// =================================================================
// Region arithmetic
// =================================================================

/// A simple integer rectangle.
struct Region {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;

    bool empty() const { return w <= 0 || h <= 0; }

    int32_t right()  const { return x + w; }
    int32_t bottom() const { return y + h; }

    bool contains(int32_t px, int32_t py) const {
        return px >= x && px < right() && py >= y && py < bottom();
    }

    bool operator==(const Region& o) const {
        return x == o.x && y == o.y && w == o.w && h == o.h;
    }
    bool operator!=(const Region& o) const { return !(*this == o); }
};

/// Check whether two regions overlap.
/// Returns true if they share at least one pixel.
inline bool checked_region_overlap(const Region& a, const Region& b) {
    if (a.empty() || b.empty()) return false;

    // Check arithmetic safety for right/bottom edges
    int32_t ar, ab, br, bb;
    if (!checked_add(a.x, a.w, ar)) return false;
    if (!checked_add(a.y, a.h, ab)) return false;
    if (!checked_add(b.x, b.w, br)) return false;
    if (!checked_add(b.y, b.h, bb)) return false;

    // No overlap if one is entirely left/right/above/below the other
    if (ar <= b.x || br <= a.x) return false;
    if (ab <= b.y || bb <= a.y) return false;
    return true;
}

/// Compute the intersection of two regions.
/// Returns an empty region if they don't overlap.
inline bool checked_region_intersect(const Region& a, const Region& b,
                                     Region& out) {
    if (a.empty() || b.empty()) {
        out = {0, 0, 0, 0};
        return true;
    }

    int32_t ar, ab, br, bb;
    if (!checked_add(a.x, a.w, ar)) return false;
    if (!checked_add(a.y, a.h, ab)) return false;
    if (!checked_add(b.x, b.w, br)) return false;
    if (!checked_add(b.y, b.h, bb)) return false;

    int32_t ix = (a.x > b.x) ? a.x : b.x;
    int32_t iy = (a.y > b.y) ? a.y : b.y;
    int32_t ir = (ar < br)   ? ar  : br;
    int32_t ib = (ab < bb)   ? ab  : bb;

    if (ir <= ix || ib <= iy) {
        out = {0, 0, 0, 0};
        return true;
    }

    int32_t rw, rh;
    if (!checked_sub(ir, ix, rw)) return false;
    if (!checked_sub(ib, iy, rh)) return false;

    out = {ix, iy, rw, rh};
    return true;
}

/// Compute the bounding-box union of two regions.
inline bool checked_region_union(const Region& a, const Region& b,
                                 Region& out) {
    if (a.empty()) { out = b; return true; }
    if (b.empty()) { out = a; return true; }

    int32_t ar, ab, br, bb;
    if (!checked_add(a.x, a.w, ar)) return false;
    if (!checked_add(a.y, a.h, ab)) return false;
    if (!checked_add(b.x, b.w, br)) return false;
    if (!checked_add(b.y, b.h, bb)) return false;

    int32_t ux = (a.x < b.x) ? a.x : b.x;
    int32_t uy = (a.y < b.y) ? a.y : b.y;
    int32_t ur = (ar > br)   ? ar  : br;
    int32_t ub = (ab > bb)   ? ab  : bb;

    int32_t uw, uh;
    if (!checked_sub(ur, ux, uw)) return false;
    if (!checked_sub(ub, uy, uh)) return false;

    out = {ux, uy, uw, uh};
    return true;
}

/// Clamp a region so it lies within [0, 0, max_w, max_h].
/// This is used to clip drawing operations to image bounds.
inline Region clamp_region(const Region& r, int32_t max_w, int32_t max_h) {
    if (r.empty()) return {0, 0, 0, 0};

    int32_t x1 = (r.x > 0) ? r.x : 0;
    int32_t y1 = (r.y > 0) ? r.y : 0;
    int32_t x2 = r.right();
    int32_t y2 = r.bottom();
    if (x2 > max_w) x2 = max_w;
    if (y2 > max_h) y2 = max_h;
    if (x2 <= x1 || y2 <= y1) return {0, 0, 0, 0};
    return {x1, y1, x2 - x1, y2 - y1};
}

/// Compute the area of a region with overflow checking.
inline bool checked_region_area(const Region& r, int64_t& area) {
    if (r.empty()) { area = 0; return true; }
    int64_t w64 = static_cast<int64_t>(r.w);
    int64_t h64 = static_cast<int64_t>(r.h);
    return checked_mul(w64, h64, area);
}

} // namespace PixelForge
