#pragma once

#include "image.h"
#include "errors.h"
#include <cstdint>

namespace PixelForge {

// ============================================================================
// Alpha Premultiplication
// ============================================================================

// Convert an RGBA image to premultiplied-alpha form:
//   R' = R * A / 255,  G' = G * A / 255,  B' = B * A / 255
PixelForgeErrorCode premultiply_alpha(Image& img);

// Convert a premultiplied-alpha RGBA image back to straight alpha:
//   R = R' * 255 / A,  G = G' * 255 / A,  B = B' * 255 / A
// Where A=0, output RGB is set to 0
PixelForgeErrorCode unpremultiply_alpha(Image& img);

// Convert between alpha modes in a single call
//   to_premultiplied=true  → premultiply
//   to_premultiplied=false → unpremultiply
PixelForgeErrorCode convert_alpha_mode(Image& img, bool to_premultiplied);

// ============================================================================
// Alpha Blending
// ============================================================================

// Blend two alpha values with an opacity factor:
//   result = src_alpha * opacity/255 + dst_alpha * (255 - src_alpha*opacity/255) / 255
uint8_t blend_alpha(uint8_t src_alpha, uint8_t dst_alpha, uint8_t opacity);

// Blend source pixel over destination pixel (Porter-Duff "over" operation)
// Both pixels assumed to be in straight-alpha RGBA
Pixel blend_pixel_over(const Pixel& src, const Pixel& dst, uint8_t opacity = 255);

// ============================================================================
// Alpha Mask
// ============================================================================

// Apply a grayscale or RGBA image as an alpha mask.
// The mask's luminance (or alpha channel if RGBA) replaces the image's alpha.
// Image and mask must have the same dimensions.
PixelForgeErrorCode apply_alpha_mask(Image& img, const Image& mask);

// Fill the alpha channel of an image with a constant value
PixelForgeErrorCode fill_alpha(Image& img, uint8_t alpha_value);

// Extract the alpha channel as a new grayscale image
PixelForgeErrorCode extract_alpha(const Image& img, Image& out_alpha);

} // namespace PixelForge
