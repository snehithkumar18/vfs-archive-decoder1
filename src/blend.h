#pragma once

#include "image.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace PixelForge {

// All 16 W3C compositing blend modes
enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity
};

// Parameters controlling how a blend operation is performed
struct BlendParams {
    BlendMode mode = BlendMode::Normal;
    float opacity = 1.0f;            // Layer opacity [0..1]
    bool clip_to_base = false;       // Restrict result to base alpha coverage
};

// Returns true if the blend mode is a non-separable (HSL-based) mode
inline bool is_nonseparable_mode(BlendMode mode) {
    return mode == BlendMode::Hue ||
           mode == BlendMode::Saturation ||
           mode == BlendMode::Color ||
           mode == BlendMode::Luminosity;
}

// Returns a human-readable name for the blend mode
const char* blend_mode_name(BlendMode mode);

// Parse a blend mode from a string name (case-insensitive).
// Returns Normal if the name is unrecognized.
BlendMode blend_mode_from_name(const char* name);

// ---------------------------------------------------------------------------
// Core per-channel blend function (separable modes only).
// base and blend are in [0..1] range.  Returns the composited value in [0..1].
// For non-separable modes this returns the base unchanged; use the HSL path.
// ---------------------------------------------------------------------------
float blend_channel(float base, float blend, BlendMode mode);

// ---------------------------------------------------------------------------
// HSL helper conversions used by non-separable blend modes.
// All values are in [0..1] range.
// ---------------------------------------------------------------------------
struct HSLColor {
    float r, g, b;   // These are actually RGB but stored in the struct
};

float hsl_luminosity(float r, float g, float b);
float hsl_saturation(float r, float g, float b);

HSLColor clip_color(float r, float g, float b);
HSLColor set_luminosity(float r, float g, float b, float lum);
HSLColor set_saturation(float r, float g, float b, float sat);

// ---------------------------------------------------------------------------
// Blend a single pixel.  dst and src point to pixel data with `channels`
// components.  Alpha is the last channel when channels == 4.
// Performs Porter-Duff "source-over" compositing with the chosen blend mode
// and opacity multiplier.
// ---------------------------------------------------------------------------
void blend_pixel(uint8_t* dst, const uint8_t* src,
                 BlendMode mode, float opacity, int channels);

// ---------------------------------------------------------------------------
// Blend a full scanline (width pixels, each `channels` bytes wide).
// This is the hot inner-loop path and processes pixels contiguously.
// ---------------------------------------------------------------------------
void blend_scanline(uint8_t* dst, const uint8_t* src,
                    int width, int channels,
                    BlendMode mode, float opacity);

// ---------------------------------------------------------------------------
// Blend the `src` image onto `dst` at position (x, y).  The src image is
// clipped to the dst bounds.  Both images must have the same channel count.
// ---------------------------------------------------------------------------
void blend_region(Image& dst, const Image& src,
                  int x, int y,
                  BlendMode mode, float opacity);

// ---------------------------------------------------------------------------
// Blend a rectangular sub-region of src onto dst.  src_rect defines the
// source rectangle {sx, sy, sw, sh} and (dx, dy) is the destination origin.
// ---------------------------------------------------------------------------
void blend_rect(Image& dst, const Image& src,
                int sx, int sy, int sw, int sh,
                int dx, int dy,
                BlendMode mode, float opacity);

// ---------------------------------------------------------------------------
// Apply a grayscale mask to modulate effective alpha during a blend.
// mask must be single-channel (Grayscale).  The mask pixel value (0-255)
// scales the source alpha before blending.
// ---------------------------------------------------------------------------
void blend_region_masked(Image& dst, const Image& src, const Image& mask,
                         int x, int y,
                         BlendMode mode, float opacity);

// ---------------------------------------------------------------------------
// Utility: clamp a float to [0..1]
// ---------------------------------------------------------------------------
inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// Convert uint8 to normalised float and back
inline float u8_to_float(uint8_t v) { return v / 255.0f; }
inline uint8_t float_to_u8(float v) {
    return static_cast<uint8_t>(clamp01(v) * 255.0f + 0.5f);
}

} // namespace PixelForge
