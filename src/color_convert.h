#pragma once

#include "color_space.h"
#include "gamma.h"
#include "image.h"
#include "errors.h"
#include <vector>
#include <cstddef>

namespace PixelForge {

// ============================================================================
// Rendering Intent (ICC v4 §7.2.15)
// ============================================================================

enum class RenderingIntent {
    Perceptual             = 0,
    RelativeColorimetric   = 1,
    Saturation             = 2,
    AbsoluteColorimetric   = 3
};

// ============================================================================
// YCbCr matrix standard
// ============================================================================

enum class YCbCrStandard {
    BT601,   // SD television
    BT709    // HD television
};

// ============================================================================
// ColorConverter — converts pixel data between color spaces
// ============================================================================

class ColorConverter {
public:
    // Construct a converter for a specific source→destination transformation.
    // The conversion pipeline is:
    //   linearize(src TF) → src RGB→XYZ → chromatic adapt → XYZ→dst RGB → encode(dst TF)
    ColorConverter(const ColorSpace& src, const ColorSpace& dst,
                   RenderingIntent intent = RenderingIntent::RelativeColorimetric);

    // Convert a single floating-point RGBA color in-place
    void convert(ColorF& pixel) const;

    // Convert a buffer of interleaved float RGBA pixels in-place
    void convert_buffer(float* data, size_t pixel_count) const;

    // Convert an 8-bit Image in-place (converts to float internally)
    PixelForgeErrorCode convert_image(Image& img) const;

    // Check if a color is inside the destination gamut
    bool is_in_gamut(const ColorF& pixel) const;

    // Clip a color to the destination gamut by clamping
    ColorF clip_to_gamut(const ColorF& pixel) const;

    // Gamut-map a color using chroma reduction in Lab space
    ColorF gamut_map_lab(const ColorF& pixel) const;

    // ========================================================================
    // Standalone conversion helpers (no ColorConverter instance needed)
    // ========================================================================

    // RGB ↔ HSL
    static HSLColorF rgb_to_hsl(const ColorF& c);
    static ColorF hsl_to_rgb(const HSLColorF& hsl);

    // RGB ↔ HSV
    static HSVColorF rgb_to_hsv(const ColorF& c);
    static ColorF hsv_to_rgb(const HSVColorF& hsv);

    // RGB ↔ YCbCr
    static void rgb_to_ycbcr(const ColorF& c, float& Y, float& Cb, float& Cr,
                              YCbCrStandard std = YCbCrStandard::BT709);
    static ColorF ycbcr_to_rgb(float Y, float Cb, float Cr,
                                YCbCrStandard std = YCbCrStandard::BT709);

    // RGB ↔ CMYK (simple UCR/GCR model)
    static CMYKColor rgb_to_cmyk(const ColorF& c);
    static ColorF cmyk_to_rgb(const CMYKColor& cmyk);

    // XYZ ↔ Lab (D50-based PCS)
    static LabColorF xyz_to_lab(const XYZColorF& xyz, const WhitePoint& wp);
    static XYZColorF lab_to_xyz(const LabColorF& lab, const WhitePoint& wp);

    // RGB → Lab (via XYZ)
    static LabColorF rgb_to_lab(const ColorF& c, const ColorSpace& cs);
    static ColorF lab_to_rgb(const LabColorF& lab, const ColorSpace& cs);

    // Compute Delta E (CIE76) between two Lab colors
    static float delta_e_76(const LabColorF& a, const LabColorF& b);

    // Compute Delta E (CIE2000) between two Lab colors (simplified)
    static float delta_e_2000(const LabColorF& a, const LabColorF& b);

private:
    ColorSpace m_src;
    ColorSpace m_dst;
    RenderingIntent m_intent;

    // Precomputed combined matrix: src_RGB_to_XYZ * adapt * XYZ_to_dst_RGB
    Mat3x3 m_combined_matrix;

    TransferFunction m_src_tf;
    TransferFunction m_dst_tf;

    // LUTs for source linearization and destination encoding
    std::vector<float> m_src_linearize_lut;
    std::vector<float> m_dst_encode_lut;

    bool m_identity;       // true if src == dst (no conversion needed)
    bool m_linear_only;    // true if only transfer function differs

    void build_pipeline();
};

} // namespace PixelForge
