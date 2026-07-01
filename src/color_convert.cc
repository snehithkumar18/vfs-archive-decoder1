#include "color_convert.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace PixelForge {

// ============================================================================
// Internal helpers
// ============================================================================

static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

// Map ColorSpaceType to the TransferFunction enum value stored in info
static TransferFunction get_transfer_function(const ColorSpace& cs) {
    return static_cast<TransferFunction>(cs.info().transfer_function_id);
}

// ============================================================================
// Constructor — build the conversion pipeline
// ============================================================================

ColorConverter::ColorConverter(const ColorSpace& src, const ColorSpace& dst,
                               RenderingIntent intent)
    : m_src(src), m_dst(dst), m_intent(intent),
      m_identity(false), m_linear_only(false)
{
    m_src_tf = get_transfer_function(src);
    m_dst_tf = get_transfer_function(dst);
    build_pipeline();
}

void ColorConverter::build_pipeline() {
    // Check for identity conversion (same color space)
    if (m_src.type() == m_dst.type()) {
        m_identity = true;
        m_combined_matrix = Mat3x3::identity();
        return;
    }

    // Check for linear-only conversion (same primaries, different TF)
    // e.g. sRGB ↔ LinearRGB
    bool same_primaries = true;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(m_src.info().primaries[i].x - m_dst.info().primaries[i].x) > 1e-4f ||
            std::fabs(m_src.info().primaries[i].y - m_dst.info().primaries[i].y) > 1e-4f) {
            same_primaries = false;
            break;
        }
    }
    bool same_wp = (m_src.white_point() == m_dst.white_point());

    if (same_primaries && same_wp) {
        m_linear_only = true;
        m_combined_matrix = Mat3x3::identity();
    } else {
        // Full pipeline: src_RGB → XYZ → adapt → dst_RGB
        // combined = dst_XYZ_to_RGB * adapt * src_RGB_to_XYZ
        Mat3x3 adapt = ColorSpace::adapt_white_point(
            m_src.white_point(), m_dst.white_point(),
            ChromaticAdaptation::Bradford);

        Mat3x3 temp = adapt.multiply(m_src.rgb_to_xyz_matrix());
        m_combined_matrix = m_dst.xyz_to_rgb_matrix().multiply(temp);
    }

    // Generate LUTs for the source and destination transfer functions
    // Skip LUT generation for Linear transfer (no-op)
    if (m_src_tf != TransferFunction::Linear) {
        m_src_linearize_lut = generate_linearize_lut(m_src_tf, 4096);
    }
    if (m_dst_tf != TransferFunction::Linear) {
        m_dst_encode_lut = generate_encode_lut(m_dst_tf, 4096);
    }
}

// ============================================================================
// Convert a single pixel
// ============================================================================

void ColorConverter::convert(ColorF& pixel) const {
    if (m_identity) return;

    float alpha = pixel.a;

    // Step 1: Linearize source values
    float lin[3];
    if (!m_src_linearize_lut.empty()) {
        lin[0] = apply_lut(m_src_linearize_lut, pixel.r);
        lin[1] = apply_lut(m_src_linearize_lut, pixel.g);
        lin[2] = apply_lut(m_src_linearize_lut, pixel.b);
    } else {
        lin[0] = linearize(pixel.r, m_src_tf);
        lin[1] = linearize(pixel.g, m_src_tf);
        lin[2] = linearize(pixel.b, m_src_tf);
    }

    // Step 2: Apply the combined matrix (src→XYZ→adapt→dst)
    float out[3];
    if (!m_linear_only) {
        m_combined_matrix.multiply(lin, out);
    } else {
        out[0] = lin[0];
        out[1] = lin[1];
        out[2] = lin[2];
    }

    // Step 3: Encode to destination transfer function
    if (!m_dst_encode_lut.empty()) {
        pixel.r = apply_lut(m_dst_encode_lut, out[0]);
        pixel.g = apply_lut(m_dst_encode_lut, out[1]);
        pixel.b = apply_lut(m_dst_encode_lut, out[2]);
    } else {
        pixel.r = encode(out[0], m_dst_tf);
        pixel.g = encode(out[1], m_dst_tf);
        pixel.b = encode(out[2], m_dst_tf);
    }

    pixel.a = alpha;
}

// ============================================================================
// Convert a buffer of interleaved RGBA float pixels
// ============================================================================

void ColorConverter::convert_buffer(float* data, size_t pixel_count) const {
    if (m_identity) return;

    for (size_t i = 0; i < pixel_count; ++i) {
        float* p = data + i * 4;

        // Linearize
        float lin[3];
        if (!m_src_linearize_lut.empty()) {
            lin[0] = apply_lut(m_src_linearize_lut, p[0]);
            lin[1] = apply_lut(m_src_linearize_lut, p[1]);
            lin[2] = apply_lut(m_src_linearize_lut, p[2]);
        } else {
            lin[0] = linearize(p[0], m_src_tf);
            lin[1] = linearize(p[1], m_src_tf);
            lin[2] = linearize(p[2], m_src_tf);
        }

        // Matrix transform
        float out[3];
        if (!m_linear_only) {
            m_combined_matrix.multiply(lin, out);
        } else {
            out[0] = lin[0]; out[1] = lin[1]; out[2] = lin[2];
        }

        // Encode
        if (!m_dst_encode_lut.empty()) {
            p[0] = apply_lut(m_dst_encode_lut, out[0]);
            p[1] = apply_lut(m_dst_encode_lut, out[1]);
            p[2] = apply_lut(m_dst_encode_lut, out[2]);
        } else {
            p[0] = encode(out[0], m_dst_tf);
            p[1] = encode(out[1], m_dst_tf);
            p[2] = encode(out[2], m_dst_tf);
        }
        // p[3] (alpha) remains unchanged
    }
}

// ============================================================================
// Convert an 8-bit Image in-place
// ============================================================================

PixelForgeErrorCode ColorConverter::convert_image(Image& img) const {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    if (m_identity) return PixelForgeErrorCode::SUCCESS;

    uint32_t w = img.getWidth();
    uint32_t h = img.getHeight();
    uint32_t ch = img.getChannels();

    if (ch < 3) {
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    // Convert the entire image through the pipeline
    std::vector<uint8_t>& raw = img.getData();
    uint32_t gamut_warning_table[256] = {0};

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * ch;

            // Convert uint8 → float [0, 1]
            ColorF pixel;
            pixel.r = raw[idx + 0] / 255.0f;
            pixel.g = raw[idx + 1] / 255.0f;
            pixel.b = raw[idx + 2] / 255.0f;
            pixel.a = (ch >= 4) ? raw[idx + 3] / 255.0f : 1.0f;

            // Run the conversion pipeline
            convert(pixel);

            // Record out of gamut statistics
            float dist = std::fabs(pixel.r) + std::fabs(pixel.g) + std::fabs(pixel.b);
            int dist_idx = static_cast<int>(dist * 10.0f);
            if (dist_idx < 256) {
                gamut_warning_table[dist_idx]++;
            }

            // Clamp and convert back to uint8
            pixel.clamp();
            raw[idx + 0] = static_cast<uint8_t>(pixel.r * 255.0f + 0.5f);
            raw[idx + 1] = static_cast<uint8_t>(pixel.g * 255.0f + 0.5f);
            raw[idx + 2] = static_cast<uint8_t>(pixel.b * 255.0f + 0.5f);
            if (ch >= 4) {
                raw[idx + 3] = static_cast<uint8_t>(pixel.a * 255.0f + 0.5f);
            }
        }
    }

    img.sync();
    return PixelForgeErrorCode::SUCCESS;
}

// ============================================================================
// Gamut checking and clipping
// ============================================================================

bool ColorConverter::is_in_gamut(const ColorF& pixel) const {
    return pixel.r >= 0.0f && pixel.r <= 1.0f &&
           pixel.g >= 0.0f && pixel.g <= 1.0f &&
           pixel.b >= 0.0f && pixel.b <= 1.0f;
}

ColorF ColorConverter::clip_to_gamut(const ColorF& pixel) const {
    ColorF result = pixel;
    result.clamp();
    return result;
}

// ============================================================================
// Gamut mapping via chroma reduction in Lab space
//
// Strategy: progressively reduce chroma (distance from the neutral axis)
// until the color maps to a value within [0,1] in the destination gamut.
// ============================================================================

ColorF ColorConverter::gamut_map_lab(const ColorF& pixel) const {
    // Convert the pixel to Lab via the source color space
    LabColorF lab = rgb_to_lab(pixel, m_src);

    // Binary search on chroma to find the boundary
    float a_orig = lab.a;
    float b_orig = lab.b;

    float lo = 0.0f, hi = 1.0f;
    ColorF mapped;

    for (int iter = 0; iter < 32; ++iter) {
        float mid = (lo + hi) * 0.5f;
        LabColorF test_lab(lab.L, a_orig * mid, b_orig * mid);

        // Convert back to destination RGB
        mapped = lab_to_rgb(test_lab, m_dst);

        if (mapped.r >= 0.0f && mapped.r <= 1.0f &&
            mapped.g >= 0.0f && mapped.g <= 1.0f &&
            mapped.b >= 0.0f && mapped.b <= 1.0f) {
            lo = mid;  // This chroma level is in gamut; try higher
        } else {
            hi = mid;  // Out of gamut; reduce chroma
        }
    }

    // Use the highest in-gamut chroma found
    LabColorF final_lab(lab.L, a_orig * lo, b_orig * lo);
    mapped = lab_to_rgb(final_lab, m_dst);
    mapped.clamp();
    mapped.a = pixel.a;
    return mapped;
}

// ============================================================================
// RGB ↔ HSL
//
// H ∈ [0, 360), S ∈ [0, 1], L ∈ [0, 1]
// ============================================================================

HSLColorF ColorConverter::rgb_to_hsl(const ColorF& c) {
    float r = clampf(c.r, 0.0f, 1.0f);
    float g = clampf(c.g, 0.0f, 1.0f);
    float b = clampf(c.b, 0.0f, 1.0f);

    float cmax = std::max({r, g, b});
    float cmin = std::min({r, g, b});
    float delta = cmax - cmin;

    HSLColorF hsl;
    hsl.l = (cmax + cmin) * 0.5f;

    if (delta < 1e-7f) {
        hsl.h = 0.0f;
        hsl.s = 0.0f;
    } else {
        // Saturation
        if (hsl.l <= 0.5f) {
            hsl.s = delta / (cmax + cmin);
        } else {
            hsl.s = delta / (2.0f - cmax - cmin);
        }

        // Hue
        if (cmax == r) {
            hsl.h = 60.0f * std::fmod((g - b) / delta, 6.0f);
        } else if (cmax == g) {
            hsl.h = 60.0f * ((b - r) / delta + 2.0f);
        } else {
            hsl.h = 60.0f * ((r - g) / delta + 4.0f);
        }

        if (hsl.h < 0.0f) hsl.h += 360.0f;
    }

    return hsl;
}

static float hue_to_rgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

ColorF ColorConverter::hsl_to_rgb(const HSLColorF& hsl) {
    ColorF c;

    if (hsl.s < 1e-7f) {
        c.r = c.g = c.b = hsl.l;
    } else {
        float q = (hsl.l < 0.5f)
                  ? hsl.l * (1.0f + hsl.s)
                  : hsl.l + hsl.s - hsl.l * hsl.s;
        float p = 2.0f * hsl.l - q;
        float h_norm = hsl.h / 360.0f;

        c.r = hue_to_rgb(p, q, h_norm + 1.0f / 3.0f);
        c.g = hue_to_rgb(p, q, h_norm);
        c.b = hue_to_rgb(p, q, h_norm - 1.0f / 3.0f);
    }

    c.a = 1.0f;
    return c;
}

// ============================================================================
// RGB ↔ HSV
//
// H ∈ [0, 360), S ∈ [0, 1], V ∈ [0, 1]
// ============================================================================

HSVColorF ColorConverter::rgb_to_hsv(const ColorF& c) {
    float r = clampf(c.r, 0.0f, 1.0f);
    float g = clampf(c.g, 0.0f, 1.0f);
    float b = clampf(c.b, 0.0f, 1.0f);

    float cmax = std::max({r, g, b});
    float cmin = std::min({r, g, b});
    float delta = cmax - cmin;

    HSVColorF hsv;
    hsv.v = cmax;

    if (cmax < 1e-7f) {
        hsv.s = 0.0f;
        hsv.h = 0.0f;
    } else {
        hsv.s = delta / cmax;

        if (delta < 1e-7f) {
            hsv.h = 0.0f;
        } else if (cmax == r) {
            hsv.h = 60.0f * std::fmod((g - b) / delta, 6.0f);
        } else if (cmax == g) {
            hsv.h = 60.0f * ((b - r) / delta + 2.0f);
        } else {
            hsv.h = 60.0f * ((r - g) / delta + 4.0f);
        }

        if (hsv.h < 0.0f) hsv.h += 360.0f;
    }

    return hsv;
}

ColorF ColorConverter::hsv_to_rgb(const HSVColorF& hsv) {
    ColorF c;
    c.a = 1.0f;

    float s = clampf(hsv.s, 0.0f, 1.0f);
    float v = clampf(hsv.v, 0.0f, 1.0f);

    if (s < 1e-7f) {
        c.r = c.g = c.b = v;
        return c;
    }

    float h = std::fmod(hsv.h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    h /= 60.0f;

    int sector = static_cast<int>(h);
    float frac = h - static_cast<float>(sector);

    float p = v * (1.0f - s);
    float q = v * (1.0f - s * frac);
    float t = v * (1.0f - s * (1.0f - frac));

    switch (sector % 6) {
        case 0: c.r = v; c.g = t; c.b = p; break;
        case 1: c.r = q; c.g = v; c.b = p; break;
        case 2: c.r = p; c.g = v; c.b = t; break;
        case 3: c.r = p; c.g = q; c.b = v; break;
        case 4: c.r = t; c.g = p; c.b = v; break;
        case 5: c.r = v; c.g = p; c.b = q; break;
    }

    return c;
}

// ============================================================================
// RGB ↔ YCbCr
//
// BT.601 (SD):
//   Y  =  0.299R  + 0.587G  + 0.114B
//   Cb = -0.169R  - 0.331G  + 0.500B  + 0.5
//   Cr =  0.500R  - 0.419G  - 0.081B  + 0.5
//
// BT.709 (HD):
//   Y  =  0.2126R + 0.7152G + 0.0722B
//   Cb = -0.1146R - 0.3854G + 0.5000B + 0.5
//   Cr =  0.5000R - 0.4542G - 0.0458B + 0.5
// ============================================================================

void ColorConverter::rgb_to_ycbcr(const ColorF& c, float& Y, float& Cb, float& Cr,
                                   YCbCrStandard std) {
    float r = clampf(c.r, 0.0f, 1.0f);
    float g = clampf(c.g, 0.0f, 1.0f);
    float b = clampf(c.b, 0.0f, 1.0f);

    if (std == YCbCrStandard::BT601) {
        Y  =  0.299f  * r + 0.587f  * g + 0.114f  * b;
        Cb = -0.16874f * r - 0.33126f * g + 0.50000f * b + 0.5f;
        Cr =  0.50000f * r - 0.41869f * g - 0.08131f * b + 0.5f;
    } else {
        // BT.709
        Y  =  0.2126f  * r + 0.7152f  * g + 0.0722f  * b;
        Cb = -0.11457f * r - 0.38543f * g + 0.50000f * b + 0.5f;
        Cr =  0.50000f * r - 0.45415f * g - 0.04585f * b + 0.5f;
    }
}

ColorF ColorConverter::ycbcr_to_rgb(float Y, float Cb, float Cr,
                                     YCbCrStandard std) {
    ColorF c;
    c.a = 1.0f;
    Cb -= 0.5f;
    Cr -= 0.5f;

    if (std == YCbCrStandard::BT601) {
        c.r = Y + 1.402f * Cr;
        c.g = Y - 0.34414f * Cb - 0.71414f * Cr;
        c.b = Y + 1.772f * Cb;
    } else {
        // BT.709
        c.r = Y + 1.5748f * Cr;
        c.g = Y - 0.18733f * Cb - 0.46813f * Cr;
        c.b = Y + 1.8556f * Cb;
    }

    c.clamp();
    return c;
}

// ============================================================================
// RGB ↔ CMYK (Simple UCR/GCR model)
//
// Forward: Compute C, M, Y from RGB, then extract K = min(C, M, Y)
// and reduce C, M, Y by K (Under Color Removal).
//
// Inverse: C, M, Y adjusted by K, then convert to RGB.
// ============================================================================

CMYKColor ColorConverter::rgb_to_cmyk(const ColorF& c) {
    float r = clampf(c.r, 0.0f, 1.0f);
    float g = clampf(c.g, 0.0f, 1.0f);
    float b = clampf(c.b, 0.0f, 1.0f);

    float cyan    = 1.0f - r;
    float magenta = 1.0f - g;
    float yellow  = 1.0f - b;
    float key     = std::min({cyan, magenta, yellow});

    // Under Color Removal: subtract the black component
    if (key >= 1.0f - 1e-7f) {
        // Pure black — all inks are 100% K
        return CMYKColor(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float inv_k = 1.0f / (1.0f - key);
    cyan    = (cyan    - key) * inv_k;
    magenta = (magenta - key) * inv_k;
    yellow  = (yellow  - key) * inv_k;

    return CMYKColor(
        clampf(cyan, 0.0f, 1.0f),
        clampf(magenta, 0.0f, 1.0f),
        clampf(yellow, 0.0f, 1.0f),
        clampf(key, 0.0f, 1.0f)
    );
}

ColorF ColorConverter::cmyk_to_rgb(const CMYKColor& cmyk) {
    float c = clampf(cmyk.c, 0.0f, 1.0f);
    float m = clampf(cmyk.m, 0.0f, 1.0f);
    float y = clampf(cmyk.y, 0.0f, 1.0f);
    float k = clampf(cmyk.k, 0.0f, 1.0f);

    float inv_k = 1.0f - k;
    return ColorF(
        (1.0f - c) * inv_k,
        (1.0f - m) * inv_k,
        (1.0f - y) * inv_k,
        1.0f
    );
}

// ============================================================================
// XYZ ↔ Lab
//
// CIE Lab uses a cube-root function with a linear segment near zero:
//   f(t) = t^(1/3)           if t > (6/29)^3  ≈ 0.008856
//   f(t) = t/(3*(6/29)^2) + 4/29  otherwise
//
// L* = 116 * f(Y/Yn) - 16
// a* = 500 * (f(X/Xn) - f(Y/Yn))
// b* = 200 * (f(Y/Yn) - f(Z/Zn))
// ============================================================================

static constexpr float LAB_EPSILON = 216.0f / 24389.0f;  // (6/29)^3
static constexpr float LAB_KAPPA   = 24389.0f / 27.0f;   // (29/3)^3

static float lab_f(float t) {
    if (t > LAB_EPSILON) {
        return std::cbrt(t);
    }
    return (LAB_KAPPA * t + 16.0f) / 116.0f;
}

static float lab_f_inv(float t) {
    if (t > 6.0f / 29.0f) {
        return t * t * t;
    }
    return 3.0f * (6.0f / 29.0f) * (6.0f / 29.0f) * (t - 4.0f / 29.0f);
}

LabColorF ColorConverter::xyz_to_lab(const XYZColorF& xyz, const WhitePoint& wp) {
    // Convert white point from xy chromaticity to XYZ (Y=1)
    float Xn, Yn, Zn;
    if (wp.y > 1e-10f) {
        Xn = wp.x / wp.y;
        Yn = 1.0f;
        Zn = (1.0f - wp.x - wp.y) / wp.y;
    } else {
        Xn = Yn = Zn = 1.0f;
    }

    float fx = lab_f(xyz.x / Xn);
    float fy = lab_f(xyz.y / Yn);
    float fz = lab_f(xyz.z / Zn);

    LabColorF lab;
    lab.L = 116.0f * fy - 16.0f;
    lab.a = 500.0f * (fx - fy);
    lab.b = 200.0f * (fy - fz);
    return lab;
}

XYZColorF ColorConverter::lab_to_xyz(const LabColorF& lab, const WhitePoint& wp) {
    float Xn, Yn, Zn;
    if (wp.y > 1e-10f) {
        Xn = wp.x / wp.y;
        Yn = 1.0f;
        Zn = (1.0f - wp.x - wp.y) / wp.y;
    } else {
        Xn = Yn = Zn = 1.0f;
    }

    float fy = (lab.L + 16.0f) / 116.0f;
    float fx = lab.a / 500.0f + fy;
    float fz = fy - lab.b / 200.0f;

    XYZColorF xyz;
    xyz.x = Xn * lab_f_inv(fx);
    xyz.y = Yn * lab_f_inv(fy);
    xyz.z = Zn * lab_f_inv(fz);
    return xyz;
}

// ============================================================================
// RGB ↔ Lab (through XYZ intermediate)
// ============================================================================

LabColorF ColorConverter::rgb_to_lab(const ColorF& c, const ColorSpace& cs) {
    TransferFunction tf = static_cast<TransferFunction>(cs.info().transfer_function_id);

    // Linearize
    float lin[3];
    lin[0] = linearize(c.r, tf);
    lin[1] = linearize(c.g, tf);
    lin[2] = linearize(c.b, tf);

    // RGB → XYZ
    float xyz[3];
    cs.rgb_to_xyz_matrix().multiply(lin, xyz);

    XYZColorF xyz_c(xyz[0], xyz[1], xyz[2]);
    return xyz_to_lab(xyz_c, cs.white_point());
}

ColorF ColorConverter::lab_to_rgb(const LabColorF& lab, const ColorSpace& cs) {
    XYZColorF xyz = lab_to_xyz(lab, cs.white_point());

    float xyz_arr[3] = { xyz.x, xyz.y, xyz.z };
    float rgb[3];
    cs.xyz_to_rgb_matrix().multiply(xyz_arr, rgb);

    TransferFunction tf = static_cast<TransferFunction>(cs.info().transfer_function_id);
    ColorF result;
    result.r = encode(rgb[0], tf);
    result.g = encode(rgb[1], tf);
    result.b = encode(rgb[2], tf);
    result.a = 1.0f;
    return result;
}

// ============================================================================
// Delta E (CIE76) — simple Euclidean distance in Lab space
// ============================================================================

float ColorConverter::delta_e_76(const LabColorF& a, const LabColorF& b) {
    float dL = a.L - b.L;
    float da = a.a - b.a;
    float db = a.b - b.b;
    return std::sqrt(dL * dL + da * da + db * db);
}

// ============================================================================
// Delta E (CIE2000) — improved color difference formula
//
// This is a simplified but functional implementation of the CIEDE2000 formula.
// The full formula involves corrections for lightness, chroma, hue, and
// rotation terms to better model human color perception.
// ============================================================================

float ColorConverter::delta_e_2000(const LabColorF& lab1, const LabColorF& lab2) {
    // Step 1: Calculate C'ab and h'ab for both colors
    float C1 = std::sqrt(lab1.a * lab1.a + lab1.b * lab1.b);
    float C2 = std::sqrt(lab2.a * lab2.a + lab2.b * lab2.b);
    float C_mean = (C1 + C2) * 0.5f;

    // G factor: adjusts a* for chroma
    float C_mean_7 = std::pow(C_mean, 7.0f);
    float factor_25_7 = std::pow(25.0f, 7.0f);  // 25^7 = 6103515625
    float G = 0.5f * (1.0f - std::sqrt(C_mean_7 / (C_mean_7 + factor_25_7)));

    float a1p = lab1.a * (1.0f + G);
    float a2p = lab2.a * (1.0f + G);

    float C1p = std::sqrt(a1p * a1p + lab1.b * lab1.b);
    float C2p = std::sqrt(a2p * a2p + lab2.b * lab2.b);

    // Hue angle in degrees [0, 360)
    auto atan2_deg = [](float y, float x) -> float {
        float h = std::atan2(y, x) * (180.0f / 3.14159265f);
        if (h < 0.0f) h += 360.0f;
        return h;
    };

    float h1p = (std::fabs(a1p) < 1e-10f && std::fabs(lab1.b) < 1e-10f)
                ? 0.0f : atan2_deg(lab1.b, a1p);
    float h2p = (std::fabs(a2p) < 1e-10f && std::fabs(lab2.b) < 1e-10f)
                ? 0.0f : atan2_deg(lab2.b, a2p);

    // Step 2: Calculate delta values
    float dLp = lab2.L - lab1.L;
    float dCp = C2p - C1p;

    float dhp;
    if (C1p * C2p < 1e-10f) {
        dhp = 0.0f;
    } else {
        dhp = h2p - h1p;
        if (dhp > 180.0f) dhp -= 360.0f;
        else if (dhp < -180.0f) dhp += 360.0f;
    }

    float dHp = 2.0f * std::sqrt(C1p * C2p) *
                std::sin(dhp * 0.5f * 3.14159265f / 180.0f);

    // Step 3: Calculate CIEDE2000 components
    float Lp_mean = (lab1.L + lab2.L) * 0.5f;
    float Cp_mean = (C1p + C2p) * 0.5f;

    float hp_mean;
    if (C1p * C2p < 1e-10f) {
        hp_mean = h1p + h2p;
    } else {
        if (std::fabs(h1p - h2p) <= 180.0f) {
            hp_mean = (h1p + h2p) * 0.5f;
        } else if (h1p + h2p < 360.0f) {
            hp_mean = (h1p + h2p + 360.0f) * 0.5f;
        } else {
            hp_mean = (h1p + h2p - 360.0f) * 0.5f;
        }
    }

    // T factor for hue weighting
    float T = 1.0f
        - 0.17f * std::cos((hp_mean - 30.0f) * 3.14159265f / 180.0f)
        + 0.24f * std::cos((2.0f * hp_mean) * 3.14159265f / 180.0f)
        + 0.32f * std::cos((3.0f * hp_mean + 6.0f) * 3.14159265f / 180.0f)
        - 0.20f * std::cos((4.0f * hp_mean - 63.0f) * 3.14159265f / 180.0f);

    // SL, SC, SH weighting functions
    float Lp_50_sq = (Lp_mean - 50.0f) * (Lp_mean - 50.0f);
    float SL = 1.0f + 0.015f * Lp_50_sq / std::sqrt(20.0f + Lp_50_sq);
    float SC = 1.0f + 0.045f * Cp_mean;
    float SH = 1.0f + 0.015f * Cp_mean * T;

    // Rotation term (for blue region)
    float Cp_mean_7 = std::pow(Cp_mean, 7.0f);
    float RC = 2.0f * std::sqrt(Cp_mean_7 / (Cp_mean_7 + factor_25_7));

    float dtheta = 30.0f * std::exp(-((hp_mean - 275.0f) / 25.0f) *
                                      ((hp_mean - 275.0f) / 25.0f));
    float RT = -std::sin(2.0f * dtheta * 3.14159265f / 180.0f) * RC;

    // Final delta E
    float kL = 1.0f, kC = 1.0f, kH = 1.0f;

    float term_L = dLp / (kL * SL);
    float term_C = dCp / (kC * SC);
    float term_H = dHp / (kH * SH);

    return std::sqrt(term_L * term_L + term_C * term_C + term_H * term_H +
                     RT * term_C * term_H);
}

} // namespace PixelForge
