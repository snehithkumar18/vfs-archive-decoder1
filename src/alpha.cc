#include "alpha.h"
#include <cstring>
#include <algorithm>

namespace PixelForge {

// ============================================================================
// Premultiply Alpha
//
// Convert from straight alpha to premultiplied alpha:
//   R' = R * A / 255
//   G' = G * A / 255
//   B' = B * A / 255
//
// Using integer math with rounding: (v * a + 127) / 255
// This avoids floating point and is exact for 8-bit values.
// ============================================================================

PixelForgeErrorCode premultiply_alpha(Image& img) {
    if (!img.isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t ch = img.getChannels();
    if (ch != 4) {
        // Only RGBA images have alpha to premultiply
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t w = img.getWidth();
    uint32_t h = img.getHeight();
    std::vector<uint8_t>& raw = img.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * 4;
            uint32_t a = raw[idx + 3];

            if (a == 255) {
                // Fully opaque — no change needed
                continue;
            }

            if (a == 0) {
                // Fully transparent — zero out RGB
                raw[idx + 0] = 0;
                raw[idx + 1] = 0;
                raw[idx + 2] = 0;
                continue;
            }

            // Premultiply with proper rounding
            raw[idx + 0] = static_cast<uint8_t>((static_cast<uint32_t>(raw[idx + 0]) * a + 127) / 255);
            raw[idx + 1] = static_cast<uint8_t>((static_cast<uint32_t>(raw[idx + 1]) * a + 127) / 255);
            raw[idx + 2] = static_cast<uint8_t>((static_cast<uint32_t>(raw[idx + 2]) * a + 127) / 255);
        }
    }

    img.sync();
    return PixelForgeErrorCode::SUCCESS;
}

// ============================================================================
// Unpremultiply Alpha
//
// Convert from premultiplied alpha back to straight alpha:
//   R = R' * 255 / A
//   G = G' * 255 / A
//   B = B' * 255 / A
//
// When A = 0, output RGB is set to 0 (color is undefined for transparent).
// ============================================================================

PixelForgeErrorCode unpremultiply_alpha(Image& img) {
    if (!img.isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t ch = img.getChannels();
    if (ch != 4) {
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t w = img.getWidth();
    uint32_t h = img.getHeight();
    std::vector<uint8_t>& raw = img.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * 4;
            uint32_t a = raw[idx + 3];

            if (a == 255) {
                // Fully opaque — already in straight form
                continue;
            }

            if (a == 0) {
                // Fully transparent — set RGB to 0 (arbitrary; color is undefined)
                raw[idx + 0] = 0;
                raw[idx + 1] = 0;
                raw[idx + 2] = 0;
                continue;
            }

            // Unpremultiply: R = min(R' * 255 / A, 255)
            uint32_t r = (static_cast<uint32_t>(raw[idx + 0]) * 255 + a / 2) / a;
            uint32_t g = (static_cast<uint32_t>(raw[idx + 1]) * 255 + a / 2) / a;
            uint32_t b = (static_cast<uint32_t>(raw[idx + 2]) * 255 + a / 2) / a;

            raw[idx + 0] = static_cast<uint8_t>(std::min(r, 255u));
            raw[idx + 1] = static_cast<uint8_t>(std::min(g, 255u));
            raw[idx + 2] = static_cast<uint8_t>(std::min(b, 255u));
        }
    }

    img.sync();
    return PixelForgeErrorCode::SUCCESS;
}

// ============================================================================
// Convert Alpha Mode
// ============================================================================

PixelForgeErrorCode convert_alpha_mode(Image& img, bool to_premultiplied) {
    if (to_premultiplied) {
        return premultiply_alpha(img);
    } else {
        return unpremultiply_alpha(img);
    }
}

// ============================================================================
// Blend Alpha Values
//
// Porter-Duff "over" alpha compositing for just the alpha channel.
// The formula is:
//   out_alpha = src_alpha * (opacity/255) + dst_alpha * (1 - src_alpha*(opacity/255))
//
// We work in 0–255 integer space with rounding.
// ============================================================================

uint8_t blend_alpha(uint8_t src_alpha, uint8_t dst_alpha, uint8_t opacity) {
    // Effective source alpha after applying opacity
    uint32_t sa = (static_cast<uint32_t>(src_alpha) * opacity + 127) / 255;

    // Porter-Duff "over" alpha:
    //   out = sa + da * (255 - sa) / 255
    uint32_t da = dst_alpha;
    uint32_t out = sa + (da * (255 - sa) + 127) / 255;

    return static_cast<uint8_t>(std::min(out, 255u));
}

// ============================================================================
// Blend Pixel Over (Porter-Duff "over" operation)
//
// For straight alpha compositing:
//   out_a = src_a*op + dst_a * (1 - src_a*op)
//   out_r = (src_r * src_a*op + dst_r * dst_a * (1 - src_a*op)) / out_a
// ============================================================================

Pixel blend_pixel_over(const Pixel& src, const Pixel& dst, uint8_t opacity) {
    // Effective source alpha
    uint32_t sa = (static_cast<uint32_t>(src.a) * opacity + 127) / 255;

    if (sa == 0) {
        return dst;
    }

    if (sa == 255) {
        return src;
    }

    uint32_t da = dst.a;
    uint32_t inv_sa = 255 - sa;

    // Output alpha
    uint32_t out_a = sa + (da * inv_sa + 127) / 255;

    if (out_a == 0) {
        return Pixel{0, 0, 0, 0};
    }

    // Blend each channel:
    //   out_c = (src_c * sa + dst_c * da * (255 - sa) / 255) / out_a
    // For better precision we compute in a larger integer space:
    uint32_t src_weight = sa;
    uint32_t dst_weight = (da * inv_sa + 127) / 255;

    Pixel result;
    result.r = static_cast<uint8_t>(
        (static_cast<uint32_t>(src.r) * src_weight +
         static_cast<uint32_t>(dst.r) * dst_weight + out_a / 2) / out_a);
    result.g = static_cast<uint8_t>(
        (static_cast<uint32_t>(src.g) * src_weight +
         static_cast<uint32_t>(dst.g) * dst_weight + out_a / 2) / out_a);
    result.b = static_cast<uint8_t>(
        (static_cast<uint32_t>(src.b) * src_weight +
         static_cast<uint32_t>(dst.b) * dst_weight + out_a / 2) / out_a);
    result.a = static_cast<uint8_t>(std::min(out_a, 255u));

    return result;
}

// ============================================================================
// Apply Alpha Mask
//
// Uses the mask's luminance (for RGB/gray) or alpha channel (for RGBA) to
// set the image's alpha. Image must be RGBA. Dimensions must match.
// ============================================================================

PixelForgeErrorCode apply_alpha_mask(Image& img, const Image& mask) {
    if (!img.isValid() || !mask.isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    if (img.getWidth() != mask.getWidth() || img.getHeight() != mask.getHeight()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    if (img.getChannels() != 4) {
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t w = img.getWidth();
    uint32_t h = img.getHeight();
    uint32_t mask_ch = mask.getChannels();
    std::vector<uint8_t>& img_data = img.getData();
    const std::vector<uint8_t>& mask_data = mask.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t img_idx = (static_cast<size_t>(y) * w + x) * 4;
            size_t mask_idx = (static_cast<size_t>(y) * w + x) * mask_ch;

            uint8_t mask_val;
            if (mask_ch == 4) {
                // Use the mask's alpha channel
                mask_val = mask_data[mask_idx + 3];
            } else if (mask_ch == 1) {
                // Grayscale mask — value is the luminance
                mask_val = mask_data[mask_idx];
            } else if (mask_ch >= 3) {
                // RGB mask — compute luminance
                // Using integer approximation of Rec. 709:
                //   L = (54*R + 183*G + 19*B + 128) >> 8
                uint32_t lum = 54u * mask_data[mask_idx + 0]
                             + 183u * mask_data[mask_idx + 1]
                             + 19u * mask_data[mask_idx + 2]
                             + 128u;
                mask_val = static_cast<uint8_t>(lum >> 8);
            } else {
                mask_val = 255;
            }

            // Multiply existing alpha by the mask value
            uint32_t combined = (static_cast<uint32_t>(img_data[img_idx + 3]) * mask_val + 127) / 255;
            img_data[img_idx + 3] = static_cast<uint8_t>(std::min(combined, 255u));
        }
    }

    img.sync();
    return PixelForgeErrorCode::SUCCESS;
}

// ============================================================================
// Fill Alpha
// ============================================================================

PixelForgeErrorCode fill_alpha(Image& img, uint8_t alpha_value) {
    if (!img.isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    if (img.getChannels() != 4) {
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t w = img.getWidth();
    uint32_t h = img.getHeight();
    std::vector<uint8_t>& raw = img.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * 4 + 3;
            raw[idx] = alpha_value;
        }
    }

    img.sync();
    return PixelForgeErrorCode::SUCCESS;
}

// ============================================================================
// Extract Alpha
//
// Creates a new grayscale image from the alpha channel of the source.
// ============================================================================

PixelForgeErrorCode extract_alpha(const Image& img, Image& out_alpha) {
    if (!img.isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    if (img.getChannels() != 4) {
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t w = img.getWidth();
    uint32_t h = img.getHeight();

    PixelForgeErrorCode err = out_alpha.allocate(w, h, PixelFormat::Grayscale);
    if (err != PixelForgeErrorCode::SUCCESS) {
        return err;
    }

    const std::vector<uint8_t>& src_data = img.getData();
    std::vector<uint8_t>& dst_data = out_alpha.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t src_idx = (static_cast<size_t>(y) * w + x) * 4 + 3;
            size_t dst_idx = static_cast<size_t>(y) * w + x;
            dst_data[dst_idx] = src_data[src_idx];
        }
    }

    out_alpha.sync();
    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
