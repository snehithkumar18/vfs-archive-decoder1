#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <cmath>

namespace PixelForge {

// ============================================================================
// Color Space Type Enumeration
// ============================================================================

enum class ColorSpaceType {
    SRGB,
    LinearRGB,
    AdobeRGB,
    ProPhotoRGB,
    DisplayP3,
    CIE_XYZ,
    CIE_Lab,
    CMYK,
    HSL,
    HSV,
    YCbCr
};

// ============================================================================
// Transfer Function Types (forward reference — full definition in gamma.h)
// ============================================================================

enum class TransferFunction : int;

// ============================================================================
// Chromatic Adaptation Method
// ============================================================================

enum class ChromaticAdaptation {
    Bradford,
    VonKries,
    XYZScaling
};

// ============================================================================
// Floating-point color types for high-precision conversion pipelines
// ============================================================================

struct ColorF {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    ColorF() = default;
    ColorF(float r_, float g_, float b_, float a_ = 1.0f)
        : r(r_), g(g_), b(b_), a(a_) {}

    // Clamp all channels to [0, 1]
    void clamp() {
        r = (r < 0.0f) ? 0.0f : (r > 1.0f ? 1.0f : r);
        g = (g < 0.0f) ? 0.0f : (g > 1.0f ? 1.0f : g);
        b = (b < 0.0f) ? 0.0f : (b > 1.0f ? 1.0f : b);
        a = (a < 0.0f) ? 0.0f : (a > 1.0f ? 1.0f : a);
    }

    // Compute perceptual luminance (Rec. 709 coefficients)
    float luminance() const {
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
};

// CIE L*a*b* color
struct LabColorF {
    float L = 0.0f;
    float a = 0.0f;
    float b = 0.0f;

    LabColorF() = default;
    LabColorF(float L_, float a_, float b_) : L(L_), a(a_), b(b_) {}
};

// CIE XYZ tristimulus values
struct XYZColorF {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    XYZColorF() = default;
    XYZColorF(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

// CMYK color (0–1 range for each channel)
struct CMYKColor {
    float c = 0.0f;
    float m = 0.0f;
    float y = 0.0f;
    float k = 0.0f;

    CMYKColor() = default;
    CMYKColor(float c_, float m_, float y_, float k_)
        : c(c_), m(m_), y(y_), k(k_) {}
};

// HSL color with float components
struct HSLColorF {
    float h = 0.0f;  // [0, 360)
    float s = 0.0f;  // [0, 1]
    float l = 0.0f;  // [0, 1]

    HSLColorF() = default;
    HSLColorF(float h_, float s_, float l_) : h(h_), s(s_), l(l_) {}
};

// HSV color with float components
struct HSVColorF {
    float h = 0.0f;  // [0, 360)
    float s = 0.0f;  // [0, 1]
    float v = 0.0f;  // [0, 1]

    HSVColorF() = default;
    HSVColorF(float h_, float s_, float v_) : h(h_), s(s_), v(v_) {}
};

// ============================================================================
// White Point (CIE xy chromaticity)
// ============================================================================

struct WhitePoint {
    float x = 0.0f;
    float y = 0.0f;

    WhitePoint() = default;
    WhitePoint(float x_, float y_) : x(x_), y(y_) {}

    bool operator==(const WhitePoint& o) const {
        return (std::fabs(x - o.x) < 1e-6f) && (std::fabs(y - o.y) < 1e-6f);
    }
    bool operator!=(const WhitePoint& o) const { return !(*this == o); }
};

// ============================================================================
// CIE xy chromaticity pair for a primary
// ============================================================================

struct Chromaticity {
    float x = 0.0f;
    float y = 0.0f;

    Chromaticity() = default;
    Chromaticity(float x_, float y_) : x(x_), y(y_) {}
};

// ============================================================================
// Color Space Info — full definition of a color space
// ============================================================================

struct ColorSpaceInfo {
    ColorSpaceType type = ColorSpaceType::SRGB;
    std::string name;
    WhitePoint white_point;
    std::array<Chromaticity, 3> primaries;  // R, G, B
    int transfer_function_id = 0;           // maps to TransferFunction enum
};

// ============================================================================
// 3×3 Matrix for RGB↔XYZ conversions
// ============================================================================

struct Mat3x3 {
    float m[3][3] = {{0}};

    Mat3x3() = default;

    // Matrix-vector multiply: result = M * v
    void multiply(const float in[3], float out[3]) const {
        out[0] = m[0][0] * in[0] + m[0][1] * in[1] + m[0][2] * in[2];
        out[1] = m[1][0] * in[0] + m[1][1] * in[1] + m[1][2] * in[2];
        out[2] = m[2][0] * in[0] + m[2][1] * in[1] + m[2][2] * in[2];
    }

    // Matrix-matrix multiply: result = this * other
    Mat3x3 multiply(const Mat3x3& other) const;

    // Compute the inverse of this matrix (returns identity on failure)
    Mat3x3 inverse() const;

    // Transpose
    Mat3x3 transpose() const;

    static Mat3x3 identity();
};

// ============================================================================
// ColorSpace — registry and factory for standard color spaces
// ============================================================================

class ColorSpace {
public:
    ColorSpace();
    explicit ColorSpace(const ColorSpaceInfo& info);

    // Factory methods for standard color spaces
    static ColorSpace sRGB();
    static ColorSpace LinearRGB();
    static ColorSpace AdobeRGB();
    static ColorSpace ProPhotoRGB();
    static ColorSpace DisplayP3();
    static ColorSpace CIE_XYZ();

    // Access color space metadata
    const ColorSpaceInfo& info() const { return m_info; }
    ColorSpaceType type() const { return m_info.type; }
    const std::string& name() const { return m_info.name; }
    const WhitePoint& white_point() const { return m_info.white_point; }

    // Precomputed conversion matrices
    const Mat3x3& rgb_to_xyz_matrix() const { return m_rgb_to_xyz; }
    const Mat3x3& xyz_to_rgb_matrix() const { return m_xyz_to_rgb; }

    // Compute the 3×3 RGB-to-XYZ matrix from the color space's primaries
    static Mat3x3 compute_rgb_to_xyz_matrix(
        const std::array<Chromaticity, 3>& primaries,
        const WhitePoint& wp);

    // Compute XYZ-to-RGB matrix (inverse of rgb_to_xyz)
    static Mat3x3 compute_xyz_to_rgb_matrix(
        const std::array<Chromaticity, 3>& primaries,
        const WhitePoint& wp);

    // Chromatic adaptation transform between white points
    static Mat3x3 adapt_white_point(
        const WhitePoint& src_wp,
        const WhitePoint& dst_wp,
        ChromaticAdaptation method = ChromaticAdaptation::Bradford);

    // Well-known white point constants
    static WhitePoint D50();
    static WhitePoint D65();
    static WhitePoint D55();
    static WhitePoint IlluminantA();
    static WhitePoint IlluminantE();

private:
    ColorSpaceInfo m_info;
    Mat3x3 m_rgb_to_xyz;
    Mat3x3 m_xyz_to_rgb;

    void compute_matrices();
};

} // namespace PixelForge
