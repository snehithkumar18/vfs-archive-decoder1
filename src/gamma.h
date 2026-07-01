#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace PixelForge {

// ============================================================================
// Transfer Function Enumeration
// ============================================================================

enum class TransferFunction : int {
    Linear    = 0,  // Identity — no gamma
    sRGB      = 1,  // IEC 61966-2-1 piecewise curve
    Gamma22   = 2,  // Simple power 2.2
    Gamma28   = 3,  // Simple power 2.8
    PQ        = 4,  // Perceptual Quantizer (SMPTE ST 2084)
    HLG       = 5,  // Hybrid Log-Gamma (ARIB STD-B67)
    AdobeRGB  = 6,  // Adobe RGB (1998): gamma 563/256 ≈ 2.19921875
    ProPhoto  = 7   // ProPhoto / ROMM: 1.8 gamma with linear toe
};

// ============================================================================
// Single-value transfer function operations
// ============================================================================

// Convert an encoded (gamma-compressed) value to linear light
float linearize(float encoded, TransferFunction tf);

// Convert a linear-light value to an encoded (gamma-compressed) value
float encode(float linear, TransferFunction tf);

// ============================================================================
// Lookup Table (LUT) generation for fast batch conversion
// ============================================================================

// Generate a LUT mapping encoded values [0..1] → linear, with 'size' entries
// Index i corresponds to encoded value i/(size-1)
std::vector<float> generate_linearize_lut(TransferFunction tf, size_t size = 4096);

// Generate a LUT mapping linear values [0..1] → encoded, with 'size' entries
std::vector<float> generate_encode_lut(TransferFunction tf, size_t size = 4096);

// Apply a precomputed LUT to a value in [0, 1] with linear interpolation
float apply_lut(const std::vector<float>& lut, float value);

// ============================================================================
// Utility: linearize / encode entire buffers
// ============================================================================

// Linearize an array of encoded values in-place
void linearize_buffer(float* data, size_t count, TransferFunction tf);

// Encode an array of linear values in-place
void encode_buffer(float* data, size_t count, TransferFunction tf);

// LUT-accelerated versions
void linearize_buffer_lut(float* data, size_t count,
                          const std::vector<float>& lut);
void encode_buffer_lut(float* data, size_t count,
                       const std::vector<float>& lut);

} // namespace PixelForge
