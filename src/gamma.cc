#include "gamma.h"
#include <cmath>
#include <algorithm>

namespace PixelForge {

// ============================================================================
// sRGB Transfer Function (IEC 61966-2-1)
//
// Linearize (EOTF):
//   if V <= 0.04045: L = V / 12.92
//   else:            L = ((V + 0.055) / 1.055)^2.4
//
// Encode (inverse EOTF / OETF):
//   if L <= 0.0031308: V = L * 12.92
//   else:              V = 1.055 * L^(1/2.4) - 0.055
// ============================================================================

static float srgb_linearize(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    if (v <= 0.04045f) {
        return v / 12.92f;
    }
    return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

static float srgb_encode(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    if (v <= 0.0031308f) {
        return v * 12.92f;
    }
    return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// ============================================================================
// Simple Gamma Power Function
// ============================================================================

static float gamma_linearize(float v, float gamma) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    return std::pow(v, gamma);
}

static float gamma_encode(float v, float gamma) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    return std::pow(v, 1.0f / gamma);
}

// ============================================================================
// Adobe RGB (1998) Transfer Function
// Gamma = 563/256 ≈ 2.19921875
// ============================================================================

static constexpr float ADOBE_RGB_GAMMA = 563.0f / 256.0f;

static float adobe_linearize(float v) {
    return gamma_linearize(v, ADOBE_RGB_GAMMA);
}

static float adobe_encode(float v) {
    return gamma_encode(v, ADOBE_RGB_GAMMA);
}

// ============================================================================
// ProPhoto / ROMM RGB Transfer Function
// Gamma = 1.8 with linear toe:
//   Linearize: if V <= 16*Et: L = V/16, else L = V^1.8
//   Encode:    if L <= Et:    V = 16*L, else V = L^(1/1.8)
//   where Et = 1/512
// ============================================================================

static constexpr float PROPHOTO_ET = 1.0f / 512.0f;
static constexpr float PROPHOTO_GAMMA = 1.8f;

static float prophoto_linearize(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    if (v <= 16.0f * PROPHOTO_ET) {
        return v / 16.0f;
    }
    return std::pow(v, PROPHOTO_GAMMA);
}

static float prophoto_encode(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;
    if (v <= PROPHOTO_ET) {
        return v * 16.0f;
    }
    return std::pow(v, 1.0f / PROPHOTO_GAMMA);
}

// ============================================================================
// PQ (Perceptual Quantizer) — SMPTE ST 2084
//
// This is the transfer function for HDR content (up to 10,000 nits).
// The signal is normalized to [0, 1] representing [0, 10000] nits.
//
// EOTF (linearize):
//   Y = ( max(V^(1/m2) - c1, 0) / (c2 - c3 * V^(1/m2)) )^(1/m1)
//
// Inverse EOTF (encode):
//   V = ( (c1 + c2 * Y^m1) / (1 + c3 * Y^m1) )^m2
//
// Constants:
//   m1 = 2610/16384 = 0.1593017578125
//   m2 = 2523/4096 * 128 = 78.84375
//   c1 = 3424/4096 = 0.8359375  (= c3 - c2 + 1)
//   c2 = 2413/4096 * 32 = 18.8515625
//   c3 = 2392/4096 * 32 = 18.6875
// ============================================================================

static constexpr float PQ_M1 = 2610.0f / 16384.0f;
static constexpr float PQ_M2 = 2523.0f / 4096.0f * 128.0f;
static constexpr float PQ_C1 = 3424.0f / 4096.0f;
static constexpr float PQ_C2 = 2413.0f / 4096.0f * 32.0f;
static constexpr float PQ_C3 = 2392.0f / 4096.0f * 32.0f;

static float pq_linearize(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;

    float Vp = std::pow(v, 1.0f / PQ_M2);
    float numerator = std::max(Vp - PQ_C1, 0.0f);
    float denominator = PQ_C2 - PQ_C3 * Vp;

    if (denominator <= 0.0f) return 0.0f;

    return std::pow(numerator / denominator, 1.0f / PQ_M1);
}

static float pq_encode(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;

    float Ym1 = std::pow(v, PQ_M1);
    float numerator = PQ_C1 + PQ_C2 * Ym1;
    float denominator = 1.0f + PQ_C3 * Ym1;

    return std::pow(numerator / denominator, PQ_M2);
}

// ============================================================================
// HLG (Hybrid Log-Gamma) — ARIB STD-B67
//
// The HLG OETF (encode):
//   if L <= 1/12: V = sqrt(3 * L)
//   else:         V = a * ln(12*L - b) + c
//
// Inverse OETF (linearize):
//   if V <= 1/2:  L = V^2 / 3
//   else:         L = (exp((V - c) / a) + b) / 12
//
// Constants:
//   a = 0.17883277
//   b = 1 - 4*a = 0.28466892
//   c = 0.5 - a*ln(4*a) = 0.55991073
// ============================================================================

static constexpr float HLG_A = 0.17883277f;
static constexpr float HLG_B = 0.28466892f;  // 1 - 4*a
static constexpr float HLG_C = 0.55991073f;

static float hlg_linearize(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;

    if (v <= 0.5f) {
        return (v * v) / 3.0f;
    }
    return (std::exp((v - HLG_C) / HLG_A) + HLG_B) / 12.0f;
}

static float hlg_encode(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v >= 1.0f) return 1.0f;

    // The crossover point: L = 1/12
    if (v <= 1.0f / 12.0f) {
        return std::sqrt(3.0f * v);
    }
    return HLG_A * std::log(12.0f * v - HLG_B) + HLG_C;
}

// ============================================================================
// Public API: linearize / encode
// ============================================================================

float linearize(float encoded, TransferFunction tf) {
    switch (tf) {
        case TransferFunction::Linear:
            return encoded;
        case TransferFunction::sRGB:
            return srgb_linearize(encoded);
        case TransferFunction::Gamma22:
            return gamma_linearize(encoded, 2.2f);
        case TransferFunction::Gamma28:
            return gamma_linearize(encoded, 2.8f);
        case TransferFunction::PQ:
            return pq_linearize(encoded);
        case TransferFunction::HLG:
            return hlg_linearize(encoded);
        case TransferFunction::AdobeRGB:
            return adobe_linearize(encoded);
        case TransferFunction::ProPhoto:
            return prophoto_linearize(encoded);
        default:
            return encoded;
    }
}

float encode(float linear, TransferFunction tf) {
    switch (tf) {
        case TransferFunction::Linear:
            return linear;
        case TransferFunction::sRGB:
            return srgb_encode(linear);
        case TransferFunction::Gamma22:
            return gamma_encode(linear, 2.2f);
        case TransferFunction::Gamma28:
            return gamma_encode(linear, 2.8f);
        case TransferFunction::PQ:
            return pq_encode(linear);
        case TransferFunction::HLG:
            return hlg_encode(linear);
        case TransferFunction::AdobeRGB:
            return adobe_encode(linear);
        case TransferFunction::ProPhoto:
            return prophoto_encode(linear);
        default:
            return linear;
    }
}

// ============================================================================
// LUT Generation
// ============================================================================

std::vector<float> generate_linearize_lut(TransferFunction tf, size_t size) {
    if (size < 2) size = 2;
    std::vector<float> lut(size);
    float inv = 1.0f / static_cast<float>(size - 1);
    for (size_t i = 0; i < size; ++i) {
        float encoded = static_cast<float>(i) * inv;
        lut[i] = linearize(encoded, tf);
    }
    return lut;
}

std::vector<float> generate_encode_lut(TransferFunction tf, size_t size) {
    if (size < 2) size = 2;
    std::vector<float> lut(size);
    float inv = 1.0f / static_cast<float>(size - 1);
    for (size_t i = 0; i < size; ++i) {
        float linear = static_cast<float>(i) * inv;
        lut[i] = encode(linear, tf);
    }
    return lut;
}

// ============================================================================
// LUT Interpolation
// ============================================================================

float apply_lut(const std::vector<float>& lut, float value) {
    if (lut.empty()) return value;

    // Clamp to [0, 1]
    if (value <= 0.0f) return lut.front();
    if (value >= 1.0f) return lut.back();

    float scaled = value * static_cast<float>(lut.size() - 1);
    size_t idx = static_cast<size_t>(scaled);
    float frac = scaled - static_cast<float>(idx);

    if (idx + 1 >= lut.size()) return lut.back();

    // Linear interpolation between adjacent entries
    return lut[idx] * (1.0f - frac) + lut[idx + 1] * frac;
}

// ============================================================================
// Buffer operations
// ============================================================================

void linearize_buffer(float* data, size_t count, TransferFunction tf) {
    if (tf == TransferFunction::Linear) return;
    for (size_t i = 0; i < count; ++i) {
        data[i] = linearize(data[i], tf);
    }
}

void encode_buffer(float* data, size_t count, TransferFunction tf) {
    if (tf == TransferFunction::Linear) return;
    for (size_t i = 0; i < count; ++i) {
        data[i] = encode(data[i], tf);
    }
}

void linearize_buffer_lut(float* data, size_t count,
                          const std::vector<float>& lut)
{
    for (size_t i = 0; i < count; ++i) {
        data[i] = apply_lut(lut, data[i]);
    }
}

void encode_buffer_lut(float* data, size_t count,
                       const std::vector<float>& lut)
{
    for (size_t i = 0; i < count; ++i) {
        data[i] = apply_lut(lut, data[i]);
    }
}

} // namespace PixelForge
