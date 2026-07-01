#include "color_space.h"
#include "gamma.h"
#include <cmath>
#include <algorithm>

namespace PixelForge {

// ============================================================================
// Mat3x3 implementation
// ============================================================================

Mat3x3 Mat3x3::identity() {
    Mat3x3 result;
    result.m[0][0] = 1.0f; result.m[0][1] = 0.0f; result.m[0][2] = 0.0f;
    result.m[1][0] = 0.0f; result.m[1][1] = 1.0f; result.m[1][2] = 0.0f;
    result.m[2][0] = 0.0f; result.m[2][1] = 0.0f; result.m[2][2] = 1.0f;
    return result;
}

Mat3x3 Mat3x3::multiply(const Mat3x3& other) const {
    Mat3x3 result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result.m[i][j] = m[i][0] * other.m[0][j]
                           + m[i][1] * other.m[1][j]
                           + m[i][2] * other.m[2][j];
        }
    }
    return result;
}

Mat3x3 Mat3x3::transpose() const {
    Mat3x3 result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            result.m[i][j] = m[j][i];
        }
    }
    return result;
}

Mat3x3 Mat3x3::inverse() const {
    // Compute inverse using cofactors and Cramer's rule
    Mat3x3 inv;

    // Cofactor matrix
    float c00 = m[1][1]*m[2][2] - m[1][2]*m[2][1];
    float c01 = m[1][2]*m[2][0] - m[1][0]*m[2][2];
    float c02 = m[1][0]*m[2][1] - m[1][1]*m[2][0];

    float c10 = m[0][2]*m[2][1] - m[0][1]*m[2][2];
    float c11 = m[0][0]*m[2][2] - m[0][2]*m[2][0];
    float c12 = m[0][1]*m[2][0] - m[0][0]*m[2][1];

    float c20 = m[0][1]*m[1][2] - m[0][2]*m[1][1];
    float c21 = m[0][2]*m[1][0] - m[0][0]*m[1][2];
    float c22 = m[0][0]*m[1][1] - m[0][1]*m[1][0];

    float det = m[0][0]*c00 + m[0][1]*c01 + m[0][2]*c02;

    if (std::fabs(det) < 1e-12f) {
        // Singular matrix — return identity as fallback
        return Mat3x3::identity();
    }

    float inv_det = 1.0f / det;

    inv.m[0][0] = c00 * inv_det;
    inv.m[0][1] = c10 * inv_det;
    inv.m[0][2] = c20 * inv_det;

    inv.m[1][0] = c01 * inv_det;
    inv.m[1][1] = c11 * inv_det;
    inv.m[1][2] = c21 * inv_det;

    inv.m[2][0] = c02 * inv_det;
    inv.m[2][1] = c12 * inv_det;
    inv.m[2][2] = c22 * inv_det;

    return inv;
}

// ============================================================================
// Well-known white points (CIE xy chromaticity)
// ============================================================================

WhitePoint ColorSpace::D50() {
    // CIE Standard Illuminant D50 — commonly used as PCS illuminant in ICC
    return WhitePoint(0.3457f, 0.3585f);
}

WhitePoint ColorSpace::D65() {
    // CIE Standard Illuminant D65 — daylight, used by sRGB, AdobeRGB, etc.
    return WhitePoint(0.3127f, 0.3290f);
}

WhitePoint ColorSpace::D55() {
    // CIE Standard Illuminant D55 — mid-morning / mid-afternoon daylight
    return WhitePoint(0.3324f, 0.3474f);
}

WhitePoint ColorSpace::IlluminantA() {
    // CIE Standard Illuminant A — incandescent / tungsten
    return WhitePoint(0.4476f, 0.4074f);
}

WhitePoint ColorSpace::IlluminantE() {
    // Equal-energy radiator
    return WhitePoint(0.3333f, 0.3333f);
}

// ============================================================================
// Helper: convert xy chromaticity + Y=1 to XYZ tristimulus
// ============================================================================

static void xy_to_XYZ(float cx, float cy, float xyz_out[3]) {
    // Given chromaticity (x, y) and Y = 1, compute X and Z
    // X = x/y, Y = 1, Z = (1-x-y)/y
    if (cy < 1e-10f) {
        xyz_out[0] = 0.0f;
        xyz_out[1] = 1.0f;
        xyz_out[2] = 0.0f;
        return;
    }
    xyz_out[0] = cx / cy;
    xyz_out[1] = 1.0f;
    xyz_out[2] = (1.0f - cx - cy) / cy;
}

// ============================================================================
// Compute 3×3 RGB-to-XYZ matrix from primaries and white point
//
// The standard method:
//   1. Convert each primary's chromaticity to XYZ (with Y=1)
//   2. Form a 3×3 matrix M_p whose columns are those XYZ values
//   3. Convert the white point to XYZ → W
//   4. Solve M_p * S = W for the scaling vector S
//   5. The RGB-to-XYZ matrix is M_p * diag(S)
// ============================================================================

Mat3x3 ColorSpace::compute_rgb_to_xyz_matrix(
    const std::array<Chromaticity, 3>& primaries,
    const WhitePoint& wp)
{
    // Step 1: Convert primaries to XYZ (Y=1)
    float r_xyz[3], g_xyz[3], b_xyz[3];
    xy_to_XYZ(primaries[0].x, primaries[0].y, r_xyz);
    xy_to_XYZ(primaries[1].x, primaries[1].y, g_xyz);
    xy_to_XYZ(primaries[2].x, primaries[2].y, b_xyz);

    // Step 2: Form the primaries matrix (columns = primary XYZ)
    // We work in row-major, but the "columns are primaries" convention means:
    //   M_p = | r_xyz[0]  g_xyz[0]  b_xyz[0] |
    //         | r_xyz[1]  g_xyz[1]  b_xyz[1] |
    //         | r_xyz[2]  g_xyz[2]  b_xyz[2] |
    Mat3x3 Mp;
    Mp.m[0][0] = r_xyz[0]; Mp.m[0][1] = g_xyz[0]; Mp.m[0][2] = b_xyz[0];
    Mp.m[1][0] = r_xyz[1]; Mp.m[1][1] = g_xyz[1]; Mp.m[1][2] = b_xyz[1];
    Mp.m[2][0] = r_xyz[2]; Mp.m[2][1] = g_xyz[2]; Mp.m[2][2] = b_xyz[2];

    // Step 3: White point → XYZ
    float w_xyz[3];
    xy_to_XYZ(wp.x, wp.y, w_xyz);

    // Step 4: Solve Mp * S = W  →  S = Mp^-1 * W
    Mat3x3 Mp_inv = Mp.inverse();
    float S[3];
    Mp_inv.multiply(w_xyz, S);

    // Step 5: Scale each column by S to get the final matrix
    Mat3x3 result;
    result.m[0][0] = S[0] * r_xyz[0];
    result.m[0][1] = S[1] * g_xyz[0];
    result.m[0][2] = S[2] * b_xyz[0];

    result.m[1][0] = S[0] * r_xyz[1];
    result.m[1][1] = S[1] * g_xyz[1];
    result.m[1][2] = S[2] * b_xyz[1];

    result.m[2][0] = S[0] * r_xyz[2];
    result.m[2][1] = S[1] * g_xyz[2];
    result.m[2][2] = S[2] * b_xyz[2];

    return result;
}

Mat3x3 ColorSpace::compute_xyz_to_rgb_matrix(
    const std::array<Chromaticity, 3>& primaries,
    const WhitePoint& wp)
{
    return compute_rgb_to_xyz_matrix(primaries, wp).inverse();
}

// ============================================================================
// Chromatic Adaptation Transforms
//
// The adaptation matrix transforms XYZ values from source illuminant to
// destination illuminant. The process is:
//   1. Convert XYZ to cone-response domain using the adaptation matrix M_A
//   2. Scale cone responses: diag(dst_cone / src_cone)
//   3. Convert back from cone domain: M_A^-1
//   Result: M_A^-1 * diag(dst_cone / src_cone) * M_A
// ============================================================================

// Bradford chromatic adaptation matrix (Hunt-Pointer-Estevez cone model)
static const float BRADFORD_M[3][3] = {
    {  0.8951f,  0.2664f, -0.1614f },
    { -0.7502f,  1.7135f,  0.0367f },
    {  0.0389f, -0.0685f,  1.0296f }
};

static const float BRADFORD_M_INV[3][3] = {
    {  0.9869929f, -0.1470543f,  0.1599627f },
    {  0.4323053f,  0.5183603f,  0.0492912f },
    { -0.0085287f,  0.0400428f,  0.9684867f }
};

// Von Kries chromatic adaptation matrix
static const float VON_KRIES_M[3][3] = {
    {  0.40024f,  0.70760f, -0.08081f },
    { -0.22630f,  1.16532f,  0.04570f },
    {  0.00000f,  0.00000f,  0.91822f }
};

static const float VON_KRIES_M_INV[3][3] = {
    {  1.8599364f, -1.1293816f,  0.2198974f },
    {  0.3611914f,  0.6388125f, -0.0000064f },
    {  0.0000000f,  0.0000000f,  1.0890636f }
};

Mat3x3 ColorSpace::adapt_white_point(
    const WhitePoint& src_wp,
    const WhitePoint& dst_wp,
    ChromaticAdaptation method)
{
    if (src_wp == dst_wp) {
        return Mat3x3::identity();
    }

    // Select the adaptation matrix
    const float (*Ma)[3];
    const float (*Ma_inv)[3];

    switch (method) {
        case ChromaticAdaptation::Bradford:
            Ma     = BRADFORD_M;
            Ma_inv = BRADFORD_M_INV;
            break;
        case ChromaticAdaptation::VonKries:
            Ma     = VON_KRIES_M;
            Ma_inv = VON_KRIES_M_INV;
            break;
        case ChromaticAdaptation::XYZScaling:
        default:
            // XYZ Scaling uses the identity adaptation matrix
            // so it just scales X, Y, Z directly
            {
                float src_xyz[3], dst_xyz[3];
                xy_to_XYZ(src_wp.x, src_wp.y, src_xyz);
                xy_to_XYZ(dst_wp.x, dst_wp.y, dst_xyz);

                Mat3x3 result;
                result.m[0][0] = dst_xyz[0] / src_xyz[0];
                result.m[1][1] = dst_xyz[1] / src_xyz[1];
                result.m[2][2] = dst_xyz[2] / src_xyz[2];
                return result;
            }
    }

    // Convert source and dest white points from xy to XYZ (Y=1)
    float src_xyz[3], dst_xyz[3];
    xy_to_XYZ(src_wp.x, src_wp.y, src_xyz);
    xy_to_XYZ(dst_wp.x, dst_wp.y, dst_xyz);

    // Transform white points to cone-response domain
    float src_cone[3] = {0}, dst_cone[3] = {0};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            src_cone[i] += Ma[i][j] * src_xyz[j];
            dst_cone[i] += Ma[i][j] * dst_xyz[j];
        }
    }

    // Build the diagonal scaling matrix in cone space
    Mat3x3 scale;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(src_cone[i]) > 1e-10f) {
            scale.m[i][i] = dst_cone[i] / src_cone[i];
        } else {
            scale.m[i][i] = 1.0f;
        }
    }

    // Build M_A and M_A^-1 as Mat3x3
    Mat3x3 Ma_mat, Ma_inv_mat;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Ma_mat.m[i][j]     = Ma[i][j];
            Ma_inv_mat.m[i][j] = Ma_inv[i][j];
        }
    }

    // Final adaptation matrix = M_A^-1 * diag(scale) * M_A
    Mat3x3 temp = scale.multiply(Ma_mat);
    return Ma_inv_mat.multiply(temp);
}

// ============================================================================
// ColorSpace constructors
// ============================================================================

ColorSpace::ColorSpace() {
    // Default to sRGB
    *this = sRGB();
}

ColorSpace::ColorSpace(const ColorSpaceInfo& info)
    : m_info(info)
{
    compute_matrices();
}

void ColorSpace::compute_matrices() {
    // Only compute matrices for RGB-based color spaces
    switch (m_info.type) {
        case ColorSpaceType::SRGB:
        case ColorSpaceType::LinearRGB:
        case ColorSpaceType::AdobeRGB:
        case ColorSpaceType::ProPhotoRGB:
        case ColorSpaceType::DisplayP3:
            m_rgb_to_xyz = compute_rgb_to_xyz_matrix(m_info.primaries, m_info.white_point);
            m_xyz_to_rgb = m_rgb_to_xyz.inverse();
            break;
        case ColorSpaceType::CIE_XYZ:
            // Identity — XYZ is the base
            m_rgb_to_xyz = Mat3x3::identity();
            m_xyz_to_rgb = Mat3x3::identity();
            break;
        default:
            // Non-RGB spaces don't use matrices directly
            m_rgb_to_xyz = Mat3x3::identity();
            m_xyz_to_rgb = Mat3x3::identity();
            break;
    }
}

// ============================================================================
// Factory: sRGB (IEC 61966-2-1)
//
// Primaries: R(0.64, 0.33), G(0.30, 0.60), B(0.15, 0.06)
// White: D65 (0.3127, 0.3290)
// Transfer: sRGB piecewise
// ============================================================================

ColorSpace ColorSpace::sRGB() {
    ColorSpaceInfo info;
    info.type = ColorSpaceType::SRGB;
    info.name = "sRGB";
    info.white_point = D65();
    info.primaries[0] = Chromaticity(0.6400f, 0.3300f);
    info.primaries[1] = Chromaticity(0.3000f, 0.6000f);
    info.primaries[2] = Chromaticity(0.1500f, 0.0600f);
    info.transfer_function_id = static_cast<int>(TransferFunction::sRGB);
    return ColorSpace(info);
}

// ============================================================================
// Factory: Linear RGB (same primaries as sRGB, but no gamma)
// ============================================================================

ColorSpace ColorSpace::LinearRGB() {
    ColorSpaceInfo info;
    info.type = ColorSpaceType::LinearRGB;
    info.name = "Linear RGB";
    info.white_point = D65();
    info.primaries[0] = Chromaticity(0.6400f, 0.3300f);
    info.primaries[1] = Chromaticity(0.3000f, 0.6000f);
    info.primaries[2] = Chromaticity(0.1500f, 0.0600f);
    info.transfer_function_id = static_cast<int>(TransferFunction::Linear);
    return ColorSpace(info);
}

// ============================================================================
// Factory: Adobe RGB (1998)
//
// Primaries: R(0.64, 0.33), G(0.21, 0.71), B(0.15, 0.06)
// White: D65
// Transfer: gamma 563/256 ≈ 2.19921875
// ============================================================================

ColorSpace ColorSpace::AdobeRGB() {
    ColorSpaceInfo info;
    info.type = ColorSpaceType::AdobeRGB;
    info.name = "Adobe RGB (1998)";
    info.white_point = D65();
    info.primaries[0] = Chromaticity(0.6400f, 0.3300f);
    info.primaries[1] = Chromaticity(0.2100f, 0.7100f);
    info.primaries[2] = Chromaticity(0.1500f, 0.0600f);
    info.transfer_function_id = static_cast<int>(TransferFunction::AdobeRGB);
    return ColorSpace(info);
}

// ============================================================================
// Factory: ProPhoto RGB (ROMM RGB)
//
// Primaries: R(0.7347, 0.2653), G(0.1596, 0.8404), B(0.0366, 0.0001)
// White: D50
// Transfer: gamma 1.8 with linear toe
// ============================================================================

ColorSpace ColorSpace::ProPhotoRGB() {
    ColorSpaceInfo info;
    info.type = ColorSpaceType::ProPhotoRGB;
    info.name = "ProPhoto RGB";
    info.white_point = D50();
    info.primaries[0] = Chromaticity(0.7347f, 0.2653f);
    info.primaries[1] = Chromaticity(0.1596f, 0.8404f);
    info.primaries[2] = Chromaticity(0.0366f, 0.0001f);
    info.transfer_function_id = static_cast<int>(TransferFunction::ProPhoto);
    return ColorSpace(info);
}

// ============================================================================
// Factory: Display P3
//
// Primaries: R(0.680, 0.320), G(0.265, 0.690), B(0.150, 0.060)
// White: D65
// Transfer: sRGB transfer function
// ============================================================================

ColorSpace ColorSpace::DisplayP3() {
    ColorSpaceInfo info;
    info.type = ColorSpaceType::DisplayP3;
    info.name = "Display P3";
    info.white_point = D65();
    info.primaries[0] = Chromaticity(0.6800f, 0.3200f);
    info.primaries[1] = Chromaticity(0.2650f, 0.6900f);
    info.primaries[2] = Chromaticity(0.1500f, 0.0600f);
    info.transfer_function_id = static_cast<int>(TransferFunction::sRGB);
    return ColorSpace(info);
}

// ============================================================================
// Factory: CIE XYZ
// ============================================================================

ColorSpace ColorSpace::CIE_XYZ() {
    ColorSpaceInfo info;
    info.type = ColorSpaceType::CIE_XYZ;
    info.name = "CIE XYZ";
    info.white_point = D50();
    // XYZ is the base — "primaries" are identity
    info.primaries[0] = Chromaticity(1.0f, 0.0f);
    info.primaries[1] = Chromaticity(0.0f, 1.0f);
    info.primaries[2] = Chromaticity(0.0f, 0.0f);
    info.transfer_function_id = static_cast<int>(TransferFunction::Linear);
    return ColorSpace(info);
}

} // namespace PixelForge
