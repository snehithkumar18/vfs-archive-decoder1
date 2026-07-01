#include "blend.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace PixelForge {

// ---------------------------------------------------------------------------
// Blend mode name table
// ---------------------------------------------------------------------------

const char* blend_mode_name(BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal:     return "Normal";
        case BlendMode::Multiply:   return "Multiply";
        case BlendMode::Screen:     return "Screen";
        case BlendMode::Overlay:    return "Overlay";
        case BlendMode::Darken:     return "Darken";
        case BlendMode::Lighten:    return "Lighten";
        case BlendMode::ColorDodge: return "ColorDodge";
        case BlendMode::ColorBurn:  return "ColorBurn";
        case BlendMode::HardLight:  return "HardLight";
        case BlendMode::SoftLight:  return "SoftLight";
        case BlendMode::Difference: return "Difference";
        case BlendMode::Exclusion:  return "Exclusion";
        case BlendMode::Hue:        return "Hue";
        case BlendMode::Saturation: return "Saturation";
        case BlendMode::Color:      return "Color";
        case BlendMode::Luminosity: return "Luminosity";
        default:                    return "Unknown";
    }
}

// Case-insensitive comparison helper
static bool streq_ci(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == *b;
}

BlendMode blend_mode_from_name(const char* name) {
    if (!name) return BlendMode::Normal;
    if (streq_ci(name, "normal"))      return BlendMode::Normal;
    if (streq_ci(name, "multiply"))    return BlendMode::Multiply;
    if (streq_ci(name, "screen"))      return BlendMode::Screen;
    if (streq_ci(name, "overlay"))     return BlendMode::Overlay;
    if (streq_ci(name, "darken"))      return BlendMode::Darken;
    if (streq_ci(name, "lighten"))     return BlendMode::Lighten;
    if (streq_ci(name, "colordodge"))  return BlendMode::ColorDodge;
    if (streq_ci(name, "colorburn"))   return BlendMode::ColorBurn;
    if (streq_ci(name, "hardlight"))   return BlendMode::HardLight;
    if (streq_ci(name, "softlight"))   return BlendMode::SoftLight;
    if (streq_ci(name, "difference"))  return BlendMode::Difference;
    if (streq_ci(name, "exclusion"))   return BlendMode::Exclusion;
    if (streq_ci(name, "hue"))         return BlendMode::Hue;
    if (streq_ci(name, "saturation"))  return BlendMode::Saturation;
    if (streq_ci(name, "color"))       return BlendMode::Color;
    if (streq_ci(name, "luminosity"))  return BlendMode::Luminosity;
    return BlendMode::Normal;
}

// ---------------------------------------------------------------------------
// HSL helper functions for non-separable blend modes
// Per the W3C compositing specification §13.4
// ---------------------------------------------------------------------------

// Luminosity of an RGB triple (Rec. 709 coefficients)
float hsl_luminosity(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

// Saturation: max component minus min component
float hsl_saturation(float r, float g, float b) {
    return std::max({r, g, b}) - std::min({r, g, b});
}

// Clip colour components to [0,1] while preserving luminosity.
// This handles the case where set_luminosity or set_saturation can push
// values out of the valid range.
HSLColor clip_color(float r, float g, float b) {
    float lum = hsl_luminosity(r, g, b);
    float mn = std::min({r, g, b});
    float mx = std::max({r, g, b});

    if (mn < 0.0f) {
        // Pull the negative components up toward lum
        float denom = lum - mn;
        if (denom > 1e-7f) {
            r = lum + (r - lum) * lum / denom;
            g = lum + (g - lum) * lum / denom;
            b = lum + (b - lum) * lum / denom;
        } else {
            r = g = b = lum;
        }
    }

    if (mx > 1.0f) {
        // Pull the high components down toward lum
        float denom = mx - lum;
        if (denom > 1e-7f) {
            r = lum + (r - lum) * (1.0f - lum) / denom;
            g = lum + (g - lum) * (1.0f - lum) / denom;
            b = lum + (b - lum) * (1.0f - lum) / denom;
        } else {
            r = g = b = lum;
        }
    }

    return {r, g, b};
}

// Set luminosity of an RGB colour to the target value
HSLColor set_luminosity(float r, float g, float b, float lum) {
    float delta = lum - hsl_luminosity(r, g, b);
    return clip_color(r + delta, g + delta, b + delta);
}

// Set saturation of an RGB colour to the target value.
// This is the W3C algorithm that sorts the components and maps them to
// a new range while preserving hue ordering.
HSLColor set_saturation(float r, float g, float b, float sat) {
    // We need to identify min, mid, max components and preserve their
    // original channel assignments.
    // Using an indirect approach with pointers to track which channel is which.
    float channels[3] = {r, g, b};
    int idx[3] = {0, 1, 2};

    // Sort indices by channel value (ascending)
    if (channels[idx[0]] > channels[idx[1]]) std::swap(idx[0], idx[1]);
    if (channels[idx[1]] > channels[idx[2]]) std::swap(idx[1], idx[2]);
    if (channels[idx[0]] > channels[idx[1]]) std::swap(idx[0], idx[1]);

    // idx[0] = min, idx[1] = mid, idx[2] = max

    float result[3];

    if (channels[idx[2]] > channels[idx[0]]) {
        // Scale mid relative to its position between min and max
        result[idx[1]] = ((channels[idx[1]] - channels[idx[0]]) * sat) /
                          (channels[idx[2]] - channels[idx[0]]);
        result[idx[2]] = sat;
    } else {
        // All components are equal — no saturation possible
        result[idx[1]] = 0.0f;
        result[idx[2]] = 0.0f;
    }
    result[idx[0]] = 0.0f;

    return {result[0], result[1], result[2]};
}

// ---------------------------------------------------------------------------
// Per-channel blend function for separable modes
// base and blend are in [0..1]
// ---------------------------------------------------------------------------

float blend_channel(float base, float blend, BlendMode mode) {
    switch (mode) {
        case BlendMode::Normal:
            // Normal mode: the blend value replaces the base
            return blend;

        case BlendMode::Multiply:
            // Darkens by multiplying base and blend
            return base * blend;

        case BlendMode::Screen:
            // Lightens: complement of the product of complements
            return 1.0f - (1.0f - base) * (1.0f - blend);

        case BlendMode::Overlay:
            // Combines Multiply and Screen based on base luminance.
            // Dark bases get Multiply, light bases get Screen.
            if (base < 0.5f) {
                return 2.0f * base * blend;
            } else {
                return 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
            }

        case BlendMode::Darken:
            // Selects the darker of base and blend
            return std::min(base, blend);

        case BlendMode::Lighten:
            // Selects the lighter of base and blend
            return std::max(base, blend);

        case BlendMode::ColorDodge:
            // Brightens base to reflect blend.
            // Division by complement of blend; clamped at extremes.
            if (base <= 0.0f) {
                return 0.0f;
            }
            if (blend >= 1.0f) {
                return 1.0f;
            }
            return std::min(1.0f, base / (1.0f - blend));

        case BlendMode::ColorBurn:
            // Darkens base to reflect blend.
            // Inverse division by blend; clamped at extremes.
            if (base >= 1.0f) {
                return 1.0f;
            }
            if (blend <= 0.0f) {
                return 0.0f;
            }
            return 1.0f - std::min(1.0f, (1.0f - base) / blend);

        case BlendMode::HardLight:
            // Like Overlay but blend controls the conditional instead of base.
            // Useful for adding strong highlights or shadows.
            if (blend < 0.5f) {
                return 2.0f * base * blend;
            } else {
                return 1.0f - 2.0f * (1.0f - base) * (1.0f - blend);
            }

        case BlendMode::SoftLight: {
            // W3C specification formula for Soft Light.
            // Subtler than Hard Light — like shining a diffuse light on the image.
            float d;
            if (base <= 0.25f) {
                d = ((16.0f * base - 12.0f) * base + 4.0f) * base;
            } else {
                d = std::sqrt(base);
            }

            if (blend <= 0.5f) {
                return base - (1.0f - 2.0f * blend) * base * (1.0f - base);
            } else {
                return base + (2.0f * blend - 1.0f) * (d - base);
            }
        }

        case BlendMode::Difference:
            // Absolute difference between base and blend.
            // Useful for comparing images — identical regions become black.
            return std::fabs(base - blend);

        case BlendMode::Exclusion:
            // Similar to Difference but lower contrast.
            // base + blend - 2 * base * blend
            return base + blend - 2.0f * base * blend;

        // Non-separable modes return base unchanged — the actual compositing
        // is done in the HSL path (blend_pixel handles these).
        case BlendMode::Hue:
        case BlendMode::Saturation:
        case BlendMode::Color:
        case BlendMode::Luminosity:
            return base;

        default:
            return blend;
    }
}

// ---------------------------------------------------------------------------
// Non-separable blend computation
// Takes base and blend RGB (normalised), returns the blended RGB.
// ---------------------------------------------------------------------------

static void blend_nonseparable(float br, float bg, float bb,
                                float sr, float sg, float sb,
                                BlendMode mode,
                                float& or_, float& og, float& ob) {
    HSLColor result;

    switch (mode) {
        case BlendMode::Hue:
            // Hue of source, saturation and luminosity of base
            result = set_saturation(sr, sg, sb, hsl_saturation(br, bg, bb));
            result = set_luminosity(result.r, result.g, result.b,
                                     hsl_luminosity(br, bg, bb));
            break;

        case BlendMode::Saturation:
            // Saturation of source, hue and luminosity of base
            result = set_saturation(br, bg, bb, hsl_saturation(sr, sg, sb));
            result = set_luminosity(result.r, result.g, result.b,
                                     hsl_luminosity(br, bg, bb));
            break;

        case BlendMode::Color:
            // Hue and saturation of source, luminosity of base
            result = set_luminosity(sr, sg, sb,
                                     hsl_luminosity(br, bg, bb));
            break;

        case BlendMode::Luminosity:
            // Luminosity of source, hue and saturation of base
            result = set_luminosity(br, bg, bb,
                                     hsl_luminosity(sr, sg, sb));
            break;

        default:
            // Should never reach here for non-separable modes
            result = {sr, sg, sb};
            break;
    }

    or_ = result.r;
    og  = result.g;
    ob  = result.b;
}

// ---------------------------------------------------------------------------
// blend_pixel — blend a single pixel using Porter-Duff source-over with
//               the specified blend mode and opacity.
//
// For channels == 4:
//   The formula is (per W3C compositing spec §9.1.3):
//     Cs = blend_func(Cb, Cs)         (per-channel or non-separable)
//     Co = αs·Cs + αb·Cb·(1 - αs)    (premultiplied output)
//     αo = αs + αb·(1 - αs)          (output alpha)
//
// For channels < 4 (no alpha):
//   Treated as fully opaque (alpha = 1).
// ---------------------------------------------------------------------------

void blend_pixel(uint8_t* dst, const uint8_t* src,
                 BlendMode mode, float opacity, int channels) {
    if (!dst || !src || channels <= 0) return;

    // Handle fully-transparent source: nothing to blend
    if (channels == 4 && src[3] == 0) return;

    // Handle zero opacity: nothing to blend
    if (opacity <= 0.0f) return;

    // Clamp opacity
    if (opacity > 1.0f) opacity = 1.0f;

    // Extract base and source colours as normalised floats
    float br, bg, bb, ba;
    float sr, sg, sb, sa;

    if (channels >= 3) {
        br = u8_to_float(dst[0]);
        bg = u8_to_float(dst[1]);
        bb = u8_to_float(dst[2]);
        ba = (channels == 4) ? u8_to_float(dst[3]) : 1.0f;

        sr = u8_to_float(src[0]);
        sg = u8_to_float(src[1]);
        sb = u8_to_float(src[2]);
        sa = (channels == 4) ? u8_to_float(src[3]) * opacity : opacity;
    } else if (channels == 1) {
        // Grayscale — treat as single-channel "luminance"
        br = u8_to_float(dst[0]);
        bg = br; bb = br;
        ba = 1.0f;

        sr = u8_to_float(src[0]);
        sg = sr; sb = sr;
        sa = opacity;
    } else {
        return;
    }

    // Compute the blended colour (before alpha compositing)
    float cr, cg, cb;

    if (is_nonseparable_mode(mode)) {
        blend_nonseparable(br, bg, bb, sr, sg, sb, mode, cr, cg, cb);
    } else {
        cr = blend_channel(br, sr, mode);
        cg = blend_channel(bg, sg, mode);
        cb = blend_channel(bb, sb, mode);
    }

    // Porter-Duff source-over compositing
    // αo = αs + αb(1 - αs)
    float ao = sa + ba * (1.0f - sa);

    if (ao < 1e-7f) {
        // Fully transparent result
        if (channels == 4) {
            dst[0] = dst[1] = dst[2] = dst[3] = 0;
        } else if (channels >= 3) {
            dst[0] = dst[1] = dst[2] = 0;
        } else {
            dst[0] = 0;
        }
        return;
    }

    // Co = (αs·Cs + αb·Cb·(1 - αs)) / αo
    float inv_ao = 1.0f / ao;
    float one_minus_sa = 1.0f - sa;

    float or_ = (sa * cr + ba * br * one_minus_sa) * inv_ao;
    float og  = (sa * cg + ba * bg * one_minus_sa) * inv_ao;
    float ob  = (sa * cb + ba * bb * one_minus_sa) * inv_ao;

    if (channels >= 3) {
        dst[0] = float_to_u8(or_);
        dst[1] = float_to_u8(og);
        dst[2] = float_to_u8(ob);
        if (channels == 4) {
            dst[3] = float_to_u8(ao);
        }
    } else if (channels == 1) {
        // Convert blended result back to grayscale
        dst[0] = float_to_u8(hsl_luminosity(or_, og, ob));
    }
}

// ---------------------------------------------------------------------------
// blend_scanline — hot inner loop for blending a row of pixels.
//
// For Normal mode at full opacity with 4-channel RGBA, we use a fast path
// that avoids per-pixel float conversions when source or dest are fully
// opaque/transparent.
// ---------------------------------------------------------------------------

void blend_scanline(uint8_t* dst, const uint8_t* src,
                    int width, int channels,
                    BlendMode mode, float opacity) {
    if (!dst || !src || width <= 0 || channels <= 0) return;
    if (opacity <= 0.0f) return;

    int stride = channels;

    // Fast path: Normal mode, full opacity, RGBA
    if (mode == BlendMode::Normal && opacity >= 1.0f && channels == 4) {
        for (int i = 0; i < width; ++i) {
            const uint8_t* sp = src + i * 4;
            uint8_t* dp = dst + i * 4;

            uint8_t sa = sp[3];

            if (sa == 255) {
                // Fully opaque source: copy directly
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = 255;
            } else if (sa == 0) {
                // Fully transparent source: skip
                continue;
            } else {
                // Partial alpha: integer-based Porter-Duff for speed
                uint32_t a_src = sa;
                uint32_t a_dst = dp[3];
                uint32_t inv_a_src = 255 - a_src;

                // Output alpha: αo = αs + αb(1 - αs)/255
                uint32_t a_out = a_src + ((a_dst * inv_a_src + 127) / 255);

                if (a_out == 0) {
                    dp[0] = dp[1] = dp[2] = dp[3] = 0;
                } else {
                    // Co = (αs·Cs + αb·Cb·(255 - αs)/255) / αo
                    for (int c = 0; c < 3; ++c) {
                        uint32_t cs = sp[c] * a_src;
                        uint32_t cb = dp[c] * a_dst * inv_a_src / 255;
                        dp[c] = static_cast<uint8_t>((cs + cb + a_out / 2) / a_out);
                    }
                    dp[3] = static_cast<uint8_t>(a_out);
                }
            }
        }
        return;
    }

    // Fast path: Normal mode, full opacity, RGB (no alpha channel)
    if (mode == BlendMode::Normal && opacity >= 1.0f && channels == 3) {
        std::memcpy(dst, src, static_cast<size_t>(width) * 3);
        return;
    }

    // Fast path: Normal mode, partial opacity, RGB
    if (mode == BlendMode::Normal && channels == 3) {
        uint32_t a256 = static_cast<uint32_t>(opacity * 256.0f + 0.5f);
        uint32_t inv_a256 = 256 - a256;
        for (int i = 0; i < width; ++i) {
            const uint8_t* sp = src + i * 3;
            uint8_t* dp = dst + i * 3;
            dp[0] = static_cast<uint8_t>((sp[0] * a256 + dp[0] * inv_a256) >> 8);
            dp[1] = static_cast<uint8_t>((sp[1] * a256 + dp[1] * inv_a256) >> 8);
            dp[2] = static_cast<uint8_t>((sp[2] * a256 + dp[2] * inv_a256) >> 8);
        }
        return;
    }

    // General path: all blend modes, all channel counts
    for (int i = 0; i < width; ++i) {
        blend_pixel(dst + i * stride, src + i * stride,
                    mode, opacity, channels);
    }
}

// ---------------------------------------------------------------------------
// blend_region — blend src image onto dst at position (x, y).
//
// Handles clipping: only the overlapping rectangle is processed.
// Both images must have the same channel count.
// ---------------------------------------------------------------------------

void blend_region(Image& dst, const Image& src,
                  int x, int y,
                  BlendMode mode, float opacity) {
    if (!dst.isValid() || !src.isValid()) return;
    if (dst.channels != src.channels) return;
    if (opacity <= 0.0f) return;

    int ch = static_cast<int>(dst.channels);

    // Compute clipped source rectangle
    int src_x = 0, src_y = 0;
    int dst_x = x, dst_y = y;
    int copy_w = static_cast<int>(src.width);
    int copy_h = static_cast<int>(src.height);

    // Clip left
    if (dst_x < 0) {
        src_x -= dst_x;
        copy_w += dst_x;
        dst_x = 0;
    }
    // Clip top
    if (dst_y < 0) {
        src_y -= dst_y;
        copy_h += dst_y;
        dst_y = 0;
    }
    // Clip right
    if (dst_x + copy_w > static_cast<int>(dst.width)) {
        copy_w = static_cast<int>(dst.width) - dst_x;
    }
    // Clip bottom
    if (dst_y + copy_h > static_cast<int>(dst.height)) {
        copy_h = static_cast<int>(dst.height) - dst_y;
    }

    if (copy_w <= 0 || copy_h <= 0) return;

    // Process scanline by scanline
    size_t dst_stride = static_cast<size_t>(dst.width) * ch;
    size_t src_stride = static_cast<size_t>(src.width) * ch;

    for (int row = 0; row < copy_h; ++row) {
        uint8_t* dp = dst.data + (dst_y + row) * dst_stride + dst_x * ch;
        const uint8_t* sp = src.data + (src_y + row) * src_stride + src_x * ch;
        blend_scanline(dp, sp, copy_w, ch, mode, opacity);
    }
}

// ---------------------------------------------------------------------------
// blend_rect — blend a sub-rectangle of src onto dst
// ---------------------------------------------------------------------------

void blend_rect(Image& dst, const Image& src,
                int sx, int sy, int sw, int sh,
                int dx, int dy,
                BlendMode mode, float opacity) {
    if (!dst.isValid() || !src.isValid()) return;
    if (dst.channels != src.channels) return;
    if (opacity <= 0.0f) return;

    int ch = static_cast<int>(dst.channels);

    // Clamp source rectangle to source image bounds
    if (sx < 0) { sw += sx; dx -= sx; sx = 0; }
    if (sy < 0) { sh += sy; dy -= sy; sy = 0; }
    if (sx + sw > static_cast<int>(src.width))  sw = static_cast<int>(src.width) - sx;
    if (sy + sh > static_cast<int>(src.height)) sh = static_cast<int>(src.height) - sy;

    // Clamp dest rectangle to dest image bounds
    if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
    if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
    if (dx + sw > static_cast<int>(dst.width))  sw = static_cast<int>(dst.width) - dx;
    if (dy + sh > static_cast<int>(dst.height)) sh = static_cast<int>(dst.height) - dy;

    if (sw <= 0 || sh <= 0) return;

    size_t dst_stride = static_cast<size_t>(dst.width) * ch;
    size_t src_stride = static_cast<size_t>(src.width) * ch;

    for (int row = 0; row < sh; ++row) {
        uint8_t* dp = dst.data + (dy + row) * dst_stride + dx * ch;
        const uint8_t* sp = src.data + (sy + row) * src_stride + sx * ch;
        blend_scanline(dp, sp, sw, ch, mode, opacity);
    }
}

// ---------------------------------------------------------------------------
// blend_region_masked — blend with a mask modulating source alpha
//
// For each pixel, the effective source alpha is:
//   effective_alpha = src_alpha * (mask_value / 255) * opacity
//
// The mask should be Grayscale (1 channel).  If it has multiple channels,
// only the first channel is used.
// ---------------------------------------------------------------------------

void blend_region_masked(Image& dst, const Image& src, const Image& mask,
                         int x, int y,
                         BlendMode mode, float opacity) {
    if (!dst.isValid() || !src.isValid() || !mask.isValid()) return;
    if (dst.channels != src.channels) return;
    if (opacity <= 0.0f) return;

    int ch = static_cast<int>(dst.channels);
    int mask_ch = static_cast<int>(mask.channels);

    // Compute clipped region (same as blend_region)
    int src_x = 0, src_y = 0;
    int dst_x = x, dst_y = y;
    int copy_w = static_cast<int>(src.width);
    int copy_h = static_cast<int>(src.height);

    // Also limit to mask size
    if (copy_w > static_cast<int>(mask.width))  copy_w = static_cast<int>(mask.width);
    if (copy_h > static_cast<int>(mask.height)) copy_h = static_cast<int>(mask.height);

    if (dst_x < 0) { src_x -= dst_x; copy_w += dst_x; dst_x = 0; }
    if (dst_y < 0) { src_y -= dst_y; copy_h += dst_y; dst_y = 0; }
    if (dst_x + copy_w > static_cast<int>(dst.width))  copy_w = static_cast<int>(dst.width) - dst_x;
    if (dst_y + copy_h > static_cast<int>(dst.height)) copy_h = static_cast<int>(dst.height) - dst_y;

    if (copy_w <= 0 || copy_h <= 0) return;

    size_t dst_stride  = static_cast<size_t>(dst.width) * ch;
    size_t src_stride  = static_cast<size_t>(src.width) * ch;
    size_t mask_stride = static_cast<size_t>(mask.width) * mask_ch;

    // We need a temporary scanline buffer where we copy source pixels
    // with modified alpha before blending onto dst
    std::vector<uint8_t> temp(static_cast<size_t>(copy_w) * ch);

    for (int row = 0; row < copy_h; ++row) {
        const uint8_t* sp = src.data + (src_y + row) * src_stride + src_x * ch;
        const uint8_t* mp = mask.data + (src_y + row) * mask_stride + src_x * mask_ch;
        uint8_t* dp = dst.data + (dst_y + row) * dst_stride + dst_x * ch;

        // Build modified source scanline with mask-adjusted alpha
        for (int col = 0; col < copy_w; ++col) {
            uint8_t mask_val = mp[col * mask_ch]; // Use first channel of mask

            if (ch == 4) {
                // Copy RGB, modulate alpha by mask
                temp[col * 4 + 0] = sp[col * 4 + 0];
                temp[col * 4 + 1] = sp[col * 4 + 1];
                temp[col * 4 + 2] = sp[col * 4 + 2];
                // Modulate: src_alpha * mask_value / 255
                uint32_t sa = sp[col * 4 + 3];
                temp[col * 4 + 3] = static_cast<uint8_t>((sa * mask_val + 127) / 255);
            } else if (ch == 3) {
                // RGB: modulate opacity by mask
                float mask_factor = mask_val / 255.0f;
                float effective_opacity = opacity * mask_factor;
                // Just copy and let blend handle opacity per-pixel
                temp[col * 3 + 0] = sp[col * 3 + 0];
                temp[col * 3 + 1] = sp[col * 3 + 1];
                temp[col * 3 + 2] = sp[col * 3 + 2];
                // For RGB we'll need per-pixel blend — see below
            } else if (ch == 1) {
                temp[col] = sp[col];
            }
        }

        if (ch == 4) {
            // The alpha has been modulated into the temp buffer; blend normally
            blend_scanline(dp, temp.data(), copy_w, ch, mode, opacity);
        } else {
            // For non-alpha channels, we need per-pixel blending with
            // mask-adjusted opacity
            for (int col = 0; col < copy_w; ++col) {
                uint8_t mask_val = mp[col * mask_ch];
                float mask_factor = mask_val / 255.0f;
                float effective_opacity = opacity * mask_factor;
                blend_pixel(dp + col * ch, sp + col * ch,
                           mode, effective_opacity, ch);
            }
        }
    }
}

} // namespace PixelForge
