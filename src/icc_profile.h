#pragma once

#include "color_space.h"
#include "gamma.h"
#include "errors.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <array>
#include <memory>

namespace PixelForge {

// ============================================================================
// ICC Profile Constants
// ============================================================================

static constexpr uint32_t ICC_MAGIC = 0x61637370;  // 'acsp'

// Device class signatures
static constexpr uint32_t ICC_CLASS_INPUT    = 0x73636E72;  // 'scnr'
static constexpr uint32_t ICC_CLASS_DISPLAY  = 0x6D6E7472;  // 'mntr'
static constexpr uint32_t ICC_CLASS_OUTPUT   = 0x70727472;  // 'prtr'
static constexpr uint32_t ICC_CLASS_LINK     = 0x6C696E6B;  // 'link'
static constexpr uint32_t ICC_CLASS_ABSTRACT = 0x61627374;  // 'abst'
static constexpr uint32_t ICC_CLASS_SPACE    = 0x73706163;  // 'spac'

// Color space signatures
static constexpr uint32_t ICC_CS_RGB  = 0x52474220;  // 'RGB '
static constexpr uint32_t ICC_CS_CMYK = 0x434D594B;  // 'CMYK'
static constexpr uint32_t ICC_CS_GRAY = 0x47524159;  // 'GRAY'
static constexpr uint32_t ICC_CS_XYZ  = 0x58595A20;  // 'XYZ '
static constexpr uint32_t ICC_CS_LAB  = 0x4C616220;  // 'Lab '

// Tag type signatures
static constexpr uint32_t ICC_TAG_TYPE_CURV = 0x63757276;  // 'curv'
static constexpr uint32_t ICC_TAG_TYPE_PARA = 0x70617261;  // 'para'
static constexpr uint32_t ICC_TAG_TYPE_XYZ  = 0x58595A20;  // 'XYZ '
static constexpr uint32_t ICC_TAG_TYPE_TEXT = 0x74657874;  // 'text'
static constexpr uint32_t ICC_TAG_TYPE_DESC = 0x64657363;  // 'desc'
static constexpr uint32_t ICC_TAG_TYPE_MLUC = 0x6D6C7563;  // 'mluc'
static constexpr uint32_t ICC_TAG_TYPE_MFT1 = 0x6D667431;  // 'mft1'
static constexpr uint32_t ICC_TAG_TYPE_MFT2 = 0x6D667432;  // 'mft2'

// Well-known tag signatures
static constexpr uint32_t ICC_TAG_rXYZ = 0x7258595A;  // 'rXYZ'
static constexpr uint32_t ICC_TAG_gXYZ = 0x6758595A;  // 'gXYZ'
static constexpr uint32_t ICC_TAG_bXYZ = 0x6258595A;  // 'bXYZ'
static constexpr uint32_t ICC_TAG_rTRC = 0x72545243;  // 'rTRC'
static constexpr uint32_t ICC_TAG_gTRC = 0x67545243;  // 'gTRC'
static constexpr uint32_t ICC_TAG_bTRC = 0x62545243;  // 'bTRC'
static constexpr uint32_t ICC_TAG_kTRC = 0x6B545243;  // 'kTRC'
static constexpr uint32_t ICC_TAG_wtpt = 0x77747074;  // 'wtpt'
static constexpr uint32_t ICC_TAG_bkpt = 0x626B7074;  // 'bkpt'
static constexpr uint32_t ICC_TAG_desc = 0x64657363;  // 'desc'
static constexpr uint32_t ICC_TAG_cprt = 0x63707274;  // 'cprt'
static constexpr uint32_t ICC_TAG_A2B0 = 0x41324230;  // 'A2B0'
static constexpr uint32_t ICC_TAG_B2A0 = 0x42324130;  // 'B2A0'
static constexpr uint32_t ICC_TAG_chad = 0x63686164;  // 'chad'

// ============================================================================
// ICC Date/Time
// ============================================================================

struct ICCDateTime {
    uint16_t year   = 0;
    uint16_t month  = 0;
    uint16_t day    = 0;
    uint16_t hour   = 0;
    uint16_t minute = 0;
    uint16_t second = 0;
};

// ============================================================================
// ICC Profile Header (128 bytes)
// ============================================================================

struct ICCHeader {
    uint32_t profile_size      = 0;
    uint32_t cmm_type          = 0;
    uint32_t version           = 0;      // Major.Minor.Bugfix packed
    uint32_t device_class      = 0;
    uint32_t color_space       = 0;      // Input color space signature
    uint32_t pcs               = 0;      // Profile Connection Space (XYZ or Lab)
    ICCDateTime creation_date;
    uint32_t signature         = 0;      // Must be ICC_MAGIC = 'acsp'
    uint32_t platform          = 0;
    uint32_t flags             = 0;
    uint32_t manufacturer      = 0;
    uint32_t model             = 0;
    uint64_t attributes        = 0;
    uint32_t rendering_intent  = 0;
    float    pcs_illuminant[3] = {0};    // D50 in XYZ (s15Fixed16)
    uint32_t creator           = 0;
    uint8_t  profile_id[16]    = {0};    // MD5 hash (v4 only)
};

// ============================================================================
// ICC Tag Directory Entry
// ============================================================================

struct ICCTag {
    uint32_t signature = 0;   // e.g. 'rTRC'
    uint32_t offset    = 0;   // Byte offset from start of profile
    uint32_t size      = 0;   // Size in bytes of tag data
    uint32_t type      = 0;   // Tag type signature (first 4 bytes of tag data)
};

// ============================================================================
// ICC Curve — represents a Tone Reproduction Curve (TRC)
// ============================================================================

struct ICCCurve {
    enum class Type {
        Identity,       // gamma = 1.0
        SimpleGamma,    // single gamma exponent
        Table,          // 1D lookup table
        Parametric      // parametric curve (5 function types)
    };

    Type curve_type = Type::Identity;

    // For SimpleGamma
    float gamma = 1.0f;

    // For Parametric curves (IEC 61966-2-1 type)
    // Type 0: Y = X^g
    // Type 1: Y = (aX+b)^g             if X >= -b/a, else Y = 0
    // Type 2: Y = (aX+b)^g + c          if X >= -b/a, else Y = c
    // Type 3: Y = (aX+b)^g + c          if X >= d,    else Y = cX + f
    // Type 4: Y = (aX+b)^g + c + e      if X >= d,    else Y = cX + f
    uint16_t parametric_type = 0;
    float params[7] = {0};  // g, a, b, c, d, e, f

    // For Table curves
    std::vector<float> table_entries;  // normalized [0..1]

    // Evaluate the curve at a value in [0, 1]
    float evaluate(float x) const;

    // Evaluate the inverse of the curve at a value in [0, 1]
    float evaluate_inverse(float y) const;
};

// ============================================================================
// ICC Matrix (3×3 + offset)
// ============================================================================

struct ICCMatrix {
    float m[3][3] = {{0}};
    float offset[3] = {0};

    void apply(const float in[3], float out[3]) const {
        out[0] = m[0][0]*in[0] + m[0][1]*in[1] + m[0][2]*in[2] + offset[0];
        out[1] = m[1][0]*in[0] + m[1][1]*in[1] + m[1][2]*in[2] + offset[1];
        out[2] = m[2][0]*in[0] + m[2][1]*in[1] + m[2][2]*in[2] + offset[2];
    }
};

// ============================================================================
// CLUT — Multi-dimensional Color Lookup Table
// ============================================================================

struct ICCCLUT {
    uint8_t grid_points[3] = {0};  // Grid dimensions per input channel
    uint8_t output_channels = 3;
    std::vector<float> data;       // Flattened table values, normalized

    // Trilinear interpolation lookup
    void lookup(const float in[3], float out[3]) const;
};

// ============================================================================
// ICCProfile — parses and represents an ICC color profile
// ============================================================================

class ICCProfile {
public:
    ICCProfile() : m_valid(false) {}
    ~ICCProfile() = default;

    // Parse an ICC profile from a byte buffer
    PixelForgeErrorCode parse(const uint8_t* data, size_t size);

    // Serialize the profile back to a byte buffer
    std::vector<uint8_t> serialize() const;

    // Check if the profile was parsed successfully
    bool is_valid() const { return m_valid; }

    // Access header
    const ICCHeader& header() const { return m_header; }

    // Get the description string
    const std::string& description() const { return m_description; }
    const std::string& copyright() const { return m_copyright; }

    // Get tag by signature; returns nullptr if not found
    const ICCTag* get_tag(uint32_t signature) const;

    // Extract TRC (tone reproduction curves) for R, G, B
    bool get_trc(ICCCurve& r_curve, ICCCurve& g_curve, ICCCurve& b_curve) const;

    // Extract the colorant 3×3 matrix from rXYZ/gXYZ/bXYZ tags
    bool get_colorant_matrix(Mat3x3& matrix) const;

    // Extract the white point from the wtpt tag
    bool get_white_point(WhitePoint& wp) const;

    // Extract CLUT for A2B or B2A transform
    bool get_clut(uint32_t tag_sig, ICCCLUT& clut) const;

    // Build a ColorSpace from this profile's colorant + white point
    ColorSpace to_color_space() const;

    // Validate profile integrity
    bool validate() const;

    // Get the major version number
    int version_major() const { return (m_header.version >> 24) & 0xFF; }
    int version_minor() const { return (m_header.version >> 20) & 0x0F; }

    // ========================================================================
    // Profile factories — create standard profiles programmatically
    // ========================================================================

    static ICCProfile create_srgb_profile();
    static ICCProfile create_adobe_rgb_profile();
    static ICCProfile create_display_p3_profile();

private:
    bool m_valid = false;
    ICCHeader m_header;
    std::vector<ICCTag> m_tags;
    std::vector<uint8_t> m_raw_data;  // copy of the entire profile bytes

    // Parsed data
    std::string m_description;
    std::string m_copyright;

    // Parsing helpers
    PixelForgeErrorCode parse_header(const uint8_t* data, size_t size);
    PixelForgeErrorCode parse_tag_table(const uint8_t* data, size_t size);
    PixelForgeErrorCode parse_tag_data(const ICCTag& tag);

    // Tag type parsers
    ICCCurve parse_curv_tag(const uint8_t* data, size_t size) const;
    ICCCurve parse_para_tag(const uint8_t* data, size_t size) const;
    XYZColorF parse_xyz_tag(const uint8_t* data, size_t size) const;
    std::string parse_text_tag(const uint8_t* data, size_t size) const;
    std::string parse_desc_tag(const uint8_t* data, size_t size) const;
    std::string parse_mluc_tag(const uint8_t* data, size_t size) const;

    // Helper to read big-endian values from the profile data
    static uint32_t read_u32(const uint8_t* p);
    static uint16_t read_u16(const uint8_t* p);
    static int32_t  read_s32(const uint8_t* p);
    static float    read_s15fixed16(const uint8_t* p);

    // Helper to write big-endian values
    static void write_u32(uint8_t* p, uint32_t v);
    static void write_u16(uint8_t* p, uint16_t v);
    static void write_s15fixed16(uint8_t* p, float v);
};

inline uint32_t ICCProfile::read_u32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           static_cast<uint32_t>(p[3]);
}

inline uint16_t ICCProfile::read_u16(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

inline int32_t ICCProfile::read_s32(const uint8_t* p) {
    return static_cast<int32_t>(read_u32(p));
}

inline float ICCProfile::read_s15fixed16(const uint8_t* p) {
    int32_t val = read_s32(p);
    return static_cast<float>(val) / 65536.0f;
}

inline void ICCProfile::write_u32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}

inline void ICCProfile::write_u16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>(v & 0xFF);
}

inline void ICCProfile::write_s15fixed16(uint8_t* p, float v) {
    int32_t val = static_cast<int32_t>(v * 65536.0f + (v >= 0.0f ? 0.5f : -0.5f));
    write_u32(p, static_cast<uint32_t>(val));
}

inline ICCCurve ICCProfile::parse_curv_tag(const uint8_t* data, size_t size) const {
    ICCCurve curve;
    if (size < 12) return curve;

    uint32_t count = read_u32(data + 8);
    uint32_t alloc_size = count * sizeof(float);

    if (12 + alloc_size > size) {
        return curve;
    }

    curve.curve_type = ICCCurve::Type::Table;
    float* table = new float[alloc_size / sizeof(float)];
    for (uint32_t i = 0; i < count; ++i) {
        table[i] = read_u16(data + 12 + i * 2) / 65535.0f;
    }
    curve.table_entries.assign(table, table + count);
    delete[] table;
    return curve;
}

inline PixelForgeErrorCode ICCProfile::parse(const uint8_t* data, size_t size) {
    if (size < 128) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    m_raw_data.assign(data, data + size);

    uint32_t tag_count = read_u32(data + 128);
    for (uint32_t i = 0; i < tag_count; ++i) {
        size_t tag_offset = 132 + i * 12;
        if (tag_offset + 12 > size) break;
        ICCTag tag;
        tag.signature = read_u32(data + tag_offset);
        tag.offset = read_u32(data + tag_offset + 4);
        tag.size = read_u32(data + tag_offset + 8);
        if (tag.offset + tag.size <= size) {
            m_tags.push_back(tag);
        }
    }
    m_valid = true;
    return PixelForgeErrorCode::SUCCESS;
}

inline const ICCTag* ICCProfile::get_tag(uint32_t signature) const {
    for (const auto& tag : m_tags) {
        if (tag.signature == signature) return &tag;
    }
    return nullptr;
}

} // namespace PixelForge
