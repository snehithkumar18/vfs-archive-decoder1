#include "bitmap_font.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace PixelForge {

// ═══════════════════════════════════════════════════════════════════════════════
// BitmapFont core methods
// ═══════════════════════════════════════════════════════════════════════════════

BitmapFont::BitmapFont() = default;
BitmapFont::~BitmapFont() = default;

const Glyph* BitmapFont::get_glyph(int codepoint) const {
    auto it = glyphs.find(codepoint);
    if (it != glyphs.end()) {
        return &it->second;
    }
    // Fallback: try '?' as a replacement character
    auto fallback = glyphs.find('?');
    if (fallback != glyphs.end()) {
        return &fallback->second;
    }
    return nullptr;
}

Glyph* BitmapFont::get_glyph_mut(int codepoint) {
    auto it = glyphs.find(codepoint);
    if (it != glyphs.end()) {
        return &it->second;
    }
    return nullptr;
}

bool BitmapFont::has_glyph(int codepoint) const {
    return glyphs.find(codepoint) != glyphs.end();
}

const FontMetrics& BitmapFont::get_metrics() const {
    return metrics;
}

void BitmapFont::add_glyph(const Glyph& glyph) {
    glyphs[glyph.codepoint] = glyph;
}

void BitmapFont::add_glyph(Glyph&& glyph) {
    int cp = glyph.codepoint;
    glyphs[cp] = std::move(glyph);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UTF-8 decoding
// ═══════════════════════════════════════════════════════════════════════════════

int BitmapFont::decode_utf8(const std::string& text, size_t& pos) {
    if (pos >= text.size()) return -1;

    uint8_t c = static_cast<uint8_t>(text[pos]);

    // Single byte (ASCII)
    if (c < 0x80) {
        pos++;
        return c;
    }

    int codepoint = 0;
    int extra_bytes = 0;

    if ((c & 0xE0) == 0xC0) {
        // Two-byte sequence
        codepoint = c & 0x1F;
        extra_bytes = 1;
    } else if ((c & 0xF0) == 0xE0) {
        // Three-byte sequence
        codepoint = c & 0x0F;
        extra_bytes = 2;
    } else if ((c & 0xF8) == 0xF0) {
        // Four-byte sequence
        codepoint = c & 0x07;
        extra_bytes = 3;
    } else {
        // Invalid lead byte — skip it
        pos++;
        return 0xFFFD; // Replacement character
    }

    pos++;
    for (int i = 0; i < extra_bytes; i++) {
        if (pos >= text.size()) return 0xFFFD;
        uint8_t cont = static_cast<uint8_t>(text[pos]);
        if ((cont & 0xC0) != 0x80) return 0xFFFD;
        codepoint = (codepoint << 6) | (cont & 0x3F);
        pos++;
    }

    return codepoint;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Text measurement
// ═══════════════════════════════════════════════════════════════════════════════

int BitmapFont::text_width(const std::string& text) const {
    int width = 0;
    int max_width = 0;
    size_t pos = 0;
    int prev_cp = -1;

    while (pos < text.size()) {
        int cp = decode_utf8(text, pos);
        if (cp < 0) break;

        if (cp == '\n') {
            max_width = std::max(max_width, width);
            width = 0;
            prev_cp = -1;
            continue;
        }
        if (cp == '\r') {
            prev_cp = -1;
            continue;
        }

        const Glyph* g = get_glyph(cp);
        if (g) {
            if (prev_cp >= 0) {
                width += get_kerning(prev_cp, cp);
            }
            width += g->advance_x;
        }
        prev_cp = cp;
    }

    return std::max(max_width, width);
}

int BitmapFont::text_height(const std::string& text) const {
    if (text.empty()) return 0;

    int line_count = 1;
    for (char c : text) {
        if (c == '\n') line_count++;
    }

    return line_count * metrics.line_height;
}

void BitmapFont::text_extents(const std::string& text, int* out_width, int* out_height) const {
    if (out_width) *out_width = text_width(text);
    if (out_height) *out_height = text_height(text);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Kerning
// ═══════════════════════════════════════════════════════════════════════════════

void BitmapFont::set_kerning(int left_cp, int right_cp, int kern_x) {
    uint32_t key = (static_cast<uint32_t>(left_cp) << 16) |
                   (static_cast<uint32_t>(right_cp) & 0xFFFF);
    m_kerning[key] = kern_x;
}

int BitmapFont::get_kerning(int left_cp, int right_cp) const {
    uint32_t key = (static_cast<uint32_t>(left_cp) << 16) |
                   (static_cast<uint32_t>(right_cp) & 0xFFFF);
    auto it = m_kerning.find(key);
    if (it != m_kerning.end()) {
        return it->second;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Glyph atlas generation
// ═══════════════════════════════════════════════════════════════════════════════

Image* BitmapFont::generate_atlas(int* atlas_cols, int* atlas_rows) const {
    if (glyphs.empty()) return nullptr;

    // Determine the maximum glyph dimensions
    int max_gw = 0, max_gh = 0;
    for (auto& pair : glyphs) {
        max_gw = std::max(max_gw, pair.second.width);
        max_gh = std::max(max_gh, pair.second.height);
    }
    if (max_gw <= 0 || max_gh <= 0) return nullptr;

    // Calculate grid dimensions: roughly square layout
    int count = static_cast<int>(glyphs.size());
    int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
    int rows = (count + cols - 1) / cols;

    // Add 1-pixel padding between glyphs
    int cell_w = max_gw + 1;
    int cell_h = max_gh + 1;
    int img_w = cols * cell_w;
    int img_h = rows * cell_h;

    // Create the atlas image as grayscale
    Image* atlas = new Image(static_cast<uint32_t>(img_w),
                             static_cast<uint32_t>(img_h),
                             PixelFormat::Grayscale);

    // Clear to black
    auto& data = atlas->getData();
    std::fill(data.begin(), data.end(), 0);
    atlas->sync();

    // Place each glyph into the grid
    int idx = 0;
    for (auto& pair : glyphs) {
        const Glyph& g = pair.second;
        int col = idx % cols;
        int row = idx / cols;
        int base_x = col * cell_w;
        int base_y = row * cell_h;

        // Copy glyph bitmap into atlas
        for (int gy = 0; gy < g.height; gy++) {
            for (int gx = 0; gx < g.width; gx++) {
                int ax = base_x + gx;
                int ay = base_y + gy;
                if (ax < img_w && ay < img_h) {
                    uint8_t val = g.bitmap[static_cast<size_t>(gy * g.width + gx)];
                    uint8_t pixel_data[1] = { val };
                    atlas->setPixel(static_cast<uint32_t>(ax),
                                    static_cast<uint32_t>(ay),
                                    pixel_data, 1);
                }
            }
        }
        idx++;
    }

    atlas->sync();

    if (atlas_cols) *atlas_cols = cols;
    if (atlas_rows) *atlas_rows = rows;
    return atlas;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PSF2 Format Loader
// ═══════════════════════════════════════════════════════════════════════════════

static constexpr uint32_t PSF2_MAGIC = 0x864ab572;
static constexpr uint32_t PSF2_HAS_UNICODE_TABLE = 0x01;

// Read a little-endian uint32 from a byte buffer
static uint32_t read_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

BitmapFont* BitmapFont::load_psf2(const uint8_t* data, size_t data_size) {
    if (!data || data_size < 32) return nullptr;

    // Parse header — 32 bytes
    PSF2Header hdr;
    hdr.magic          = read_le32(data + 0);
    hdr.version        = read_le32(data + 4);
    hdr.header_size    = read_le32(data + 8);
    hdr.flags          = read_le32(data + 12);
    hdr.glyph_count    = read_le32(data + 16);
    hdr.bytes_per_glyph = read_le32(data + 20);
    hdr.height         = read_le32(data + 24);
    hdr.width          = read_le32(data + 28);

    // Validate magic number
    if (hdr.magic != PSF2_MAGIC) return nullptr;

    // Sanity checks
    if (hdr.width == 0 || hdr.height == 0 || hdr.glyph_count == 0) return nullptr;
    if (hdr.bytes_per_glyph == 0) return nullptr;
    if (hdr.header_size < 32) return nullptr;

    // Verify the file is large enough to contain all glyphs
    size_t bitmap_section_size = static_cast<size_t>(hdr.glyph_count) * hdr.bytes_per_glyph;
    if (hdr.header_size + bitmap_section_size > data_size) return nullptr;

    BitmapFont* font = new BitmapFont();
    font->name = "PSF2 Font";
    font->size = static_cast<int>(hdr.height);
    font->is_monospace = true;

    // Set font metrics
    font->metrics.ascent = static_cast<int>(hdr.height);
    font->metrics.descent = 0;
    font->metrics.line_height = static_cast<int>(hdr.height);
    font->metrics.max_advance = static_cast<int>(hdr.width);
    font->metrics.em_size = static_cast<int>(hdr.height);
    font->metrics.underline_pos = static_cast<int>(hdr.height) - 1;
    font->metrics.underline_thickness = 1;

    int glyph_w = static_cast<int>(hdr.width);
    int glyph_h = static_cast<int>(hdr.height);
    // Number of bytes per row: each row is padded to full bytes, MSB-first
    int row_bytes = (glyph_w + 7) / 8;

    const uint8_t* bitmap_data = data + hdr.header_size;

    for (uint32_t i = 0; i < hdr.glyph_count; i++) {
        Glyph g;
        g.codepoint = static_cast<int>(i); // Default mapping: glyph index = codepoint
        g.width = glyph_w;
        g.height = glyph_h;
        g.advance_x = glyph_w;
        g.advance_y = 0;
        g.bearing_x = 0;
        g.bearing_y = glyph_h;
        g.bitmap.resize(static_cast<size_t>(glyph_w) * glyph_h, 0);

        const uint8_t* glyph_bits = bitmap_data + i * hdr.bytes_per_glyph;

        // Convert packed 1-bit-per-pixel to 8-bit-per-pixel alpha
        for (int row = 0; row < glyph_h; row++) {
            const uint8_t* row_data = glyph_bits + row * row_bytes;
            for (int col = 0; col < glyph_w; col++) {
                int byte_idx = col / 8;
                int bit_idx = 7 - (col % 8); // MSB first
                bool pixel_set = false;
                if (byte_idx < row_bytes) {
                    pixel_set = (row_data[byte_idx] >> bit_idx) & 1;
                }
                g.bitmap[static_cast<size_t>(row * glyph_w + col)] =
                    pixel_set ? 255 : 0;
            }
        }

        font->add_glyph(std::move(g));
    }

    // Parse Unicode table if present
    if (hdr.flags & PSF2_HAS_UNICODE_TABLE) {
        size_t unicode_offset = hdr.header_size + bitmap_section_size;
        if (unicode_offset < data_size) {
            // The unicode table maps glyph indices to Unicode codepoints.
            // Format: for each glyph, a sequence of UTF-8 codepoints terminated by 0xFF.
            // Sequences starting with 0xFE are combining sequences (we skip those).
            uint32_t glyph_idx = 0;
            size_t pos = unicode_offset;

            while (pos < data_size && glyph_idx < hdr.glyph_count) {
                uint8_t byte = data[pos];

                if (byte == 0xFF) {
                    // End of entries for this glyph
                    glyph_idx++;
                    pos++;
                    continue;
                }

                if (byte == 0xFE) {
                    // Start of combining sequence — skip until 0xFF
                    pos++;
                    while (pos < data_size && data[pos] != 0xFF) {
                        pos++;
                    }
                    continue;
                }

                // Decode a UTF-8 codepoint from the unicode table
                int cp = 0;
                if (byte < 0x80) {
                    cp = byte;
                    pos++;
                } else if ((byte & 0xE0) == 0xC0 && pos + 1 < data_size) {
                    cp = ((byte & 0x1F) << 6) | (data[pos + 1] & 0x3F);
                    pos += 2;
                } else if ((byte & 0xF0) == 0xE0 && pos + 2 < data_size) {
                    cp = ((byte & 0x0F) << 12)
                       | ((data[pos + 1] & 0x3F) << 6)
                       | (data[pos + 2] & 0x3F);
                    pos += 3;
                } else if ((byte & 0xF8) == 0xF0 && pos + 3 < data_size) {
                    cp = ((byte & 0x07) << 18)
                       | ((data[pos + 1] & 0x3F) << 12)
                       | ((data[pos + 2] & 0x3F) << 6)
                       | (data[pos + 3] & 0x3F);
                    pos += 4;
                } else {
                    // Invalid UTF-8, skip byte
                    pos++;
                    continue;
                }

                // Map this Unicode codepoint to the glyph at glyph_idx
                if (cp != static_cast<int>(glyph_idx) && cp > 0) {
                    // Copy glyph data from the original index to the new codepoint
                    auto it = font->glyphs.find(static_cast<int>(glyph_idx));
                    if (it != font->glyphs.end()) {
                        Glyph mapped = it->second;
                        mapped.codepoint = cp;
                        font->glyphs[cp] = std::move(mapped);
                    }
                }
            }
        }
    }

    return font;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BDF Format Loader
// ═══════════════════════════════════════════════════════════════════════════════

// Parse a hex character to its integer value
static int hex_char_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

// Parse a hex string to bytes
static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t byte = static_cast<uint8_t>(
            (hex_char_val(hex[i]) << 4) | hex_char_val(hex[i + 1]));
        result.push_back(byte);
    }
    return result;
}

// Trim whitespace from both ends of a string
static std::string trim_string(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' ||
                                 s[start] == '\r' || s[start] == '\n')) {
        start++;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                            s[end - 1] == '\r' || s[end - 1] == '\n')) {
        end--;
    }
    return s.substr(start, end - start);
}

// Extract the first word from a line
static std::string first_word(const std::string& line) {
    size_t pos = 0;
    while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
        pos++;
    }
    return line.substr(0, pos);
}

// Extract everything after the first word
static std::string rest_of_line(const std::string& line) {
    size_t pos = 0;
    while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
        pos++;
    }
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        pos++;
    }
    return line.substr(pos);
}

// Parse an integer from a string, returns 0 on failure
static int parse_int(const std::string& s) {
    if (s.empty()) return 0;
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}

// Split a string by whitespace into tokens
static std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

BitmapFont* BitmapFont::load_bdf(const uint8_t* data, size_t data_size) {
    if (!data || data_size == 0) return nullptr;

    // Convert to string for line-by-line parsing
    std::string content(reinterpret_cast<const char*>(data), data_size);

    // Split into lines
    std::vector<std::string> lines;
    {
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            // Strip trailing \r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
    }

    if (lines.empty()) return nullptr;

    // Verify this is a BDF file
    if (lines[0].substr(0, 9) != "STARTFONT") return nullptr;

    BitmapFont* font = new BitmapFont();
    font->name = "BDF Font";

    // Font bounding box defaults
    int fbb_w = 0, fbb_h = 0, fbb_x = 0, fbb_y = 0;

    // Current glyph being parsed
    bool in_char = false;
    bool in_bitmap = false;
    Glyph current_glyph;
    int bbx_w = 0, bbx_h = 0, bbx_x = 0, bbx_y = 0;
    int dwidth_x = 0;
    std::vector<std::string> bitmap_lines;

    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& line = lines[i];
        std::string keyword = first_word(line);
        std::string args = rest_of_line(line);

        if (keyword == "FONT") {
            font->name = trim_string(args);
        } else if (keyword == "SIZE") {
            auto parts = split_whitespace(args);
            if (!parts.empty()) {
                font->size = parse_int(parts[0]);
            }
        } else if (keyword == "FONTBOUNDINGBOX") {
            auto parts = split_whitespace(args);
            if (parts.size() >= 4) {
                fbb_w = parse_int(parts[0]);
                fbb_h = parse_int(parts[1]);
                fbb_x = parse_int(parts[2]);
                fbb_y = parse_int(parts[3]);
            }
            font->metrics.ascent = fbb_h + fbb_y;
            font->metrics.descent = fbb_y;
            font->metrics.line_height = fbb_h;
            font->metrics.max_advance = fbb_w;
            font->metrics.em_size = fbb_h;
        } else if (keyword == "FONT_ASCENT") {
            font->metrics.ascent = parse_int(args);
        } else if (keyword == "FONT_DESCENT") {
            font->metrics.descent = -parse_int(args);
        } else if (keyword == "STARTCHAR") {
            in_char = true;
            in_bitmap = false;
            current_glyph = Glyph();
            current_glyph.codepoint = -1;
            bbx_w = fbb_w;
            bbx_h = fbb_h;
            bbx_x = fbb_x;
            bbx_y = fbb_y;
            dwidth_x = fbb_w;
            bitmap_lines.clear();
        } else if (keyword == "ENCODING" && in_char) {
            current_glyph.codepoint = parse_int(args);
        } else if (keyword == "SWIDTH" && in_char) {
            // Scalable width — we primarily use DWIDTH for pixel width
        } else if (keyword == "DWIDTH" && in_char) {
            auto parts = split_whitespace(args);
            if (!parts.empty()) {
                dwidth_x = parse_int(parts[0]);
            }
        } else if (keyword == "BBX" && in_char) {
            auto parts = split_whitespace(args);
            if (parts.size() >= 4) {
                bbx_w = parse_int(parts[0]);
                bbx_h = parse_int(parts[1]);
                bbx_x = parse_int(parts[2]);
                bbx_y = parse_int(parts[3]);
            }
        } else if (keyword == "BITMAP" && in_char) {
            in_bitmap = true;
            bitmap_lines.clear();
        } else if (keyword == "ENDCHAR" && in_char) {
            // Process the completed glyph
            if (current_glyph.codepoint >= 0) {
                current_glyph.width = bbx_w;
                current_glyph.height = bbx_h;
                current_glyph.advance_x = dwidth_x;
                current_glyph.advance_y = 0;
                current_glyph.bearing_x = bbx_x;
                current_glyph.bearing_y = bbx_y + bbx_h; // BDF bearing_y is from baseline

                // Convert hex bitmap scanlines to 8-bit alpha
                current_glyph.bitmap.resize(
                    static_cast<size_t>(bbx_w) * bbx_h, 0);

                for (int row = 0; row < bbx_h && row < static_cast<int>(bitmap_lines.size()); row++) {
                    std::vector<uint8_t> row_bytes = hex_to_bytes(
                        trim_string(bitmap_lines[static_cast<size_t>(row)]));

                    for (int col = 0; col < bbx_w; col++) {
                        int byte_idx = col / 8;
                        int bit_idx = 7 - (col % 8);
                        bool pixel_set = false;
                        if (byte_idx < static_cast<int>(row_bytes.size())) {
                            pixel_set = (row_bytes[static_cast<size_t>(byte_idx)] >> bit_idx) & 1;
                        }
                        current_glyph.bitmap[static_cast<size_t>(row * bbx_w + col)] =
                            pixel_set ? 255 : 0;
                    }
                }

                font->add_glyph(std::move(current_glyph));
            }

            in_char = false;
            in_bitmap = false;
        } else if (in_bitmap && in_char) {
            // This line is a hex-encoded bitmap scanline
            bitmap_lines.push_back(line);
        } else if (keyword == "ENDFONT") {
            break;
        }
    }

    // Update metrics if not set from properties
    if (font->metrics.line_height == 0 && fbb_h > 0) {
        font->metrics.line_height = fbb_h;
    }

    // Detect monospace: all glyphs have the same advance
    bool mono = true;
    int first_advance = -1;
    for (auto& pair : font->glyphs) {
        if (pair.second.advance_x > 0) {
            if (first_advance < 0) {
                first_advance = pair.second.advance_x;
            } else if (pair.second.advance_x != first_advance) {
                mono = false;
                break;
            }
        }
    }
    font->is_monospace = mono;

    return font;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Glyph drawing helpers for built-in font generation
// ═══════════════════════════════════════════════════════════════════════════════

void BitmapFont::glyph_set_pixel(Glyph& g, int x, int y, uint8_t val) {
    if (x >= 0 && x < g.width && y >= 0 && y < g.height) {
        g.bitmap[static_cast<size_t>(y * g.width + x)] = val;
    }
}

void BitmapFont::glyph_draw_hline(Glyph& g, int x0, int x1, int y, uint8_t val) {
    for (int x = x0; x <= x1; x++) {
        glyph_set_pixel(g, x, y, val);
    }
}

void BitmapFont::glyph_draw_vline(Glyph& g, int x, int y0, int y1, uint8_t val) {
    for (int y = y0; y <= y1; y++) {
        glyph_set_pixel(g, x, y, val);
    }
}

void BitmapFont::glyph_draw_rect(Glyph& g, int x0, int y0, int x1, int y1, uint8_t val) {
    glyph_draw_hline(g, x0, x1, y0, val);
    glyph_draw_hline(g, x0, x1, y1, val);
    glyph_draw_vline(g, x0, y0, y1, val);
    glyph_draw_vline(g, x1, y0, y1, val);
}

void BitmapFont::glyph_fill_rect(Glyph& g, int x0, int y0, int x1, int y1, uint8_t val) {
    for (int y = y0; y <= y1; y++) {
        glyph_draw_hline(g, x0, x1, y, val);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Built-in font generation: procedurally generated bitmap font
// ═══════════════════════════════════════════════════════════════════════════════

// Each digit/letter/punctuation is drawn as a series of horizontal and vertical
// line segments within a cell grid. The cell is 6 wide x 10 tall for size<=10,
// or 8 wide x 16 tall for size>10.

static Glyph make_blank_glyph(int cp, int w, int h) {
    Glyph g;
    g.codepoint = cp;
    g.width = w;
    g.height = h;
    g.advance_x = w;
    g.advance_y = 0;
    g.bearing_x = 0;
    g.bearing_y = h;
    g.bitmap.resize(static_cast<size_t>(w) * h, 0);
    return g;
}

void BitmapFont::generate_glyph_digit(Glyph& g, int digit, int w, int h) {
    // All digits drawn in a 7-segment-display style within the glyph cell
    // Segments: top, top-left, top-right, middle, bottom-left, bottom-right, bottom
    int x0 = 1, x1 = w - 2;
    int y0 = 1, ymid = h / 2, y1 = h - 2;

    // Which segments are on for each digit (top, tl, tr, mid, bl, br, bot)
    //                           top  tl   tr   mid  bl   br   bot
    static const bool segs[10][7] = {
        { true,  true,  true,  false, true,  true,  true  }, // 0
        { false, false, true,  false, false, true,  false }, // 1
        { true,  false, true,  true,  true,  false, true  }, // 2
        { true,  false, true,  true,  false, true,  true  }, // 3
        { false, true,  true,  true,  false, true,  false }, // 4
        { true,  true,  false, true,  false, true,  true  }, // 5
        { true,  true,  false, true,  true,  true,  true  }, // 6
        { true,  false, true,  false, false, true,  false }, // 7
        { true,  true,  true,  true,  true,  true,  true  }, // 8
        { true,  true,  true,  true,  false, true,  true  }, // 9
    };

    if (digit < 0 || digit > 9) return;

    const bool* s = segs[digit];

    if (s[0]) glyph_draw_hline(g, x0, x1, y0, 255);       // top
    if (s[1]) glyph_draw_vline(g, x0, y0, ymid, 255);     // top-left
    if (s[2]) glyph_draw_vline(g, x1, y0, ymid, 255);     // top-right
    if (s[3]) glyph_draw_hline(g, x0, x1, ymid, 255);     // middle
    if (s[4]) glyph_draw_vline(g, x0, ymid, y1, 255);     // bottom-left
    if (s[5]) glyph_draw_vline(g, x1, ymid, y1, 255);     // bottom-right
    if (s[6]) glyph_draw_hline(g, x0, x1, y1, 255);       // bottom
}

void BitmapFont::generate_glyph_letter_upper(Glyph& g, int letter_index, int w, int h) {
    int x0 = 1, x1 = w - 2;
    int y0 = 1, ymid = h / 2, y1 = h - 2;

    switch (letter_index) {
    case 0: // A
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        break;
    case 1: // B
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x1, y0, ymid, 255);
        glyph_draw_vline(g, x1, ymid, y1, 255);
        break;
    case 2: // C
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 3: // D
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x1, y0 + 1, y1 - 1, 255);
        break;
    case 4: // E
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 5: // F
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        break;
    case 6: // G
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x1, ymid, y1, 255);
        glyph_draw_hline(g, w / 2, x1, ymid, 255);
        break;
    case 7: // H
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        break;
    case 8: // I
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, w / 2, y0, y1, 255);
        break;
    case 9: // J
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x0, ymid + 1, y1, 255);
        break;
    case 10: // K
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, ymid - 1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x1, ymid + 1, y1, 255);
        break;
    case 11: // L
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 12: // M
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        glyph_set_pixel(g, x0 + 1, y0 + 1, 255);
        glyph_set_pixel(g, x1 - 1, y0 + 1, 255);
        glyph_set_pixel(g, w / 2, y0 + 2, 255);
        break;
    case 13: // N
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        // Diagonal approximation
        for (int i = 0; i <= y1 - y0; i++) {
            int dx = (i * (x1 - x0)) / (y1 - y0);
            glyph_set_pixel(g, x0 + dx, y0 + i, 255);
        }
        break;
    case 14: // O
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        break;
    case 15: // P
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x1, y0, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        break;
    case 16: // Q
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        glyph_set_pixel(g, x1 - 1, y1 - 1, 255);
        break;
    case 17: // R
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x1, y0, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x1, ymid + 1, y1, 255);
        break;
    case 18: // S
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x0, y0, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x1, ymid, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 19: // T
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, w / 2, y0, y1, 255);
        break;
    case 20: // U
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 21: // V
        glyph_draw_vline(g, x0, y0, y1 - 2, 255);
        glyph_draw_vline(g, x1, y0, y1 - 2, 255);
        glyph_set_pixel(g, x0 + 1, y1 - 1, 255);
        glyph_set_pixel(g, x1 - 1, y1 - 1, 255);
        glyph_set_pixel(g, w / 2, y1, 255);
        break;
    case 22: // W
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_vline(g, x1, y0, y1, 255);
        glyph_draw_vline(g, w / 2, ymid, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 23: // X
        for (int i = 0; i <= y1 - y0; i++) {
            int dx = (i * (x1 - x0)) / (y1 - y0);
            glyph_set_pixel(g, x0 + dx, y0 + i, 255);
            glyph_set_pixel(g, x1 - dx, y0 + i, 255);
        }
        break;
    case 24: // Y
        glyph_draw_vline(g, x0, y0, ymid, 255);
        glyph_draw_vline(g, x1, y0, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, w / 2, ymid, y1, 255);
        break;
    case 25: // Z
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        for (int i = 0; i <= y1 - y0; i++) {
            int dx = (i * (x1 - x0)) / (y1 - y0);
            glyph_set_pixel(g, x1 - dx, y0 + i, 255);
        }
        break;
    }
}

void BitmapFont::generate_glyph_letter_lower(Glyph& g, int letter_index, int w, int h) {
    // Lowercase letters are drawn in the lower portion of the cell
    int x0 = 1, x1 = w - 2;
    int y_top = h / 3;       // Lowercase starts lower
    int ymid = (y_top + h - 2) / 2;
    int y1 = h - 2;

    switch (letter_index) {
    case 0: // a
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x0, ymid, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 1: // b
        glyph_draw_vline(g, x0, 1, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 2: // c
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 3: // d
        glyph_draw_vline(g, x1, 1, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 4: // e
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_vline(g, x1, y_top, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 5: // f
        glyph_draw_hline(g, w / 2, x1, 1, 255);
        glyph_draw_vline(g, w / 2, 1, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        break;
    case 6: // g
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x0, y_top, ymid, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 7: // h
        glyph_draw_vline(g, x0, 1, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x1, ymid, y1, 255);
        break;
    case 8: // i
        glyph_set_pixel(g, w / 2, y_top - 1, 255);
        glyph_draw_vline(g, w / 2, y_top + 1, y1, 255);
        break;
    case 9: // j
        glyph_set_pixel(g, x1, y_top - 1, 255);
        glyph_draw_vline(g, x1, y_top + 1, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 10: // k
        glyph_draw_vline(g, x0, 1, y1, 255);
        glyph_draw_vline(g, x1, y_top, ymid - 1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x1, ymid + 1, y1, 255);
        break;
    case 11: // l
        glyph_draw_vline(g, w / 2, 1, y1, 255);
        break;
    case 12: // m
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_vline(g, w / 2, y_top, y1, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        break;
    case 13: // n
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        break;
    case 14: // o
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        break;
    case 15: // p
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x1, y_top, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        break;
    case 16: // q
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x0, y_top, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        break;
    case 17: // r
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        break;
    case 18: // s
        glyph_draw_hline(g, x0, x1, y_top, 255);
        glyph_draw_vline(g, x0, y_top, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x1, ymid, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 19: // t
        glyph_draw_vline(g, w / 2, 1, y1, 255);
        glyph_draw_hline(g, x0, x1, y_top, 255);
        break;
    case 20: // u
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 21: // v
        glyph_draw_vline(g, x0, y_top, y1 - 1, 255);
        glyph_draw_vline(g, x1, y_top, y1 - 1, 255);
        glyph_set_pixel(g, w / 2, y1, 255);
        break;
    case 22: // w
        glyph_draw_vline(g, x0, y_top, y1, 255);
        glyph_draw_vline(g, w / 2, ymid, y1, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 23: // x
        for (int i = 0; i <= y1 - y_top; i++) {
            int dx = (i * (x1 - x0)) / std::max(1, y1 - y_top);
            glyph_set_pixel(g, x0 + dx, y_top + i, 255);
            glyph_set_pixel(g, x1 - dx, y_top + i, 255);
        }
        break;
    case 24: // y
        glyph_draw_vline(g, x0, y_top, ymid, 255);
        glyph_draw_vline(g, x1, y_top, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 25: // z
        glyph_draw_hline(g, x0, x1, y_top, 255);
        for (int i = 0; i <= y1 - y_top; i++) {
            int dx = (i * (x1 - x0)) / std::max(1, y1 - y_top);
            glyph_set_pixel(g, x1 - dx, y_top + i, 255);
        }
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    }
}

void BitmapFont::generate_glyph_punctuation(Glyph& g, int codepoint, int w, int h) {
    int x0 = 1, x1 = w - 2;
    int y0 = 1, ymid = h / 2, y1 = h - 2;
    int cx = w / 2, cy = h / 2;

    switch (codepoint) {
    case ' ':
        // Space — no pixels
        break;
    case '!':
        glyph_draw_vline(g, cx, y0, y1 - 2, 255);
        glyph_set_pixel(g, cx, y1, 255);
        break;
    case '"':
        glyph_draw_vline(g, cx - 1, y0, y0 + 2, 255);
        glyph_draw_vline(g, cx + 1, y0, y0 + 2, 255);
        break;
    case '#':
        glyph_draw_vline(g, cx - 1, y0 + 1, y1 - 1, 255);
        glyph_draw_vline(g, cx + 1, y0 + 1, y1 - 1, 255);
        glyph_draw_hline(g, x0, x1, ymid - 1, 255);
        glyph_draw_hline(g, x0, x1, ymid + 1, 255);
        break;
    case '$':
        glyph_draw_hline(g, x0, x1, y0 + 1, 255);
        glyph_draw_vline(g, x0, y0 + 1, ymid, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, x1, ymid, y1 - 1, 255);
        glyph_draw_hline(g, x0, x1, y1 - 1, 255);
        glyph_draw_vline(g, cx, y0, y1, 255);
        break;
    case '%':
        glyph_set_pixel(g, x0, y0, 255);
        glyph_set_pixel(g, x1, y1, 255);
        for (int i = 0; i <= y1 - y0; i++) {
            int dx = (i * (x1 - x0)) / std::max(1, y1 - y0);
            glyph_set_pixel(g, x1 - dx, y0 + i, 255);
        }
        break;
    case '&':
        glyph_draw_hline(g, x0, cx, y0, 255);
        glyph_draw_vline(g, x0, y0, ymid, 255);
        glyph_draw_hline(g, x0, cx, ymid, 255);
        glyph_draw_vline(g, x0, ymid, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        glyph_draw_vline(g, x1, ymid, y1, 255);
        break;
    case '\'':
        glyph_draw_vline(g, cx, y0, y0 + 2, 255);
        break;
    case '(':
        glyph_draw_vline(g, cx, y0, y1, 255);
        glyph_set_pixel(g, cx + 1, y0, 255);
        glyph_set_pixel(g, cx + 1, y1, 255);
        break;
    case ')':
        glyph_draw_vline(g, cx, y0, y1, 255);
        glyph_set_pixel(g, cx - 1, y0, 255);
        glyph_set_pixel(g, cx - 1, y1, 255);
        break;
    case '*':
        glyph_set_pixel(g, cx, ymid, 255);
        glyph_set_pixel(g, cx - 1, ymid - 1, 255);
        glyph_set_pixel(g, cx + 1, ymid - 1, 255);
        glyph_set_pixel(g, cx - 1, ymid + 1, 255);
        glyph_set_pixel(g, cx + 1, ymid + 1, 255);
        break;
    case '+':
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, cx, ymid - 2, ymid + 2, 255);
        break;
    case ',':
        glyph_set_pixel(g, cx, y1, 255);
        glyph_set_pixel(g, cx - 1, y1 + 1, 255);
        break;
    case '-':
        glyph_draw_hline(g, x0, x1, ymid, 255);
        break;
    case '.':
        glyph_set_pixel(g, cx, y1, 255);
        break;
    case '/':
        for (int i = 0; i <= y1 - y0; i++) {
            int dx = (i * (x1 - x0)) / std::max(1, y1 - y0);
            glyph_set_pixel(g, x1 - dx, y0 + i, 255);
        }
        break;
    case ':':
        glyph_set_pixel(g, cx, ymid - 2, 255);
        glyph_set_pixel(g, cx, ymid + 2, 255);
        break;
    case ';':
        glyph_set_pixel(g, cx, ymid - 2, 255);
        glyph_set_pixel(g, cx, ymid + 2, 255);
        glyph_set_pixel(g, cx - 1, ymid + 3, 255);
        break;
    case '<':
        glyph_set_pixel(g, x1, y0 + 1, 255);
        glyph_set_pixel(g, cx, ymid, 255);
        glyph_set_pixel(g, x0, ymid, 255);
        glyph_set_pixel(g, cx, ymid, 255);
        glyph_set_pixel(g, x1, y1 - 1, 255);
        glyph_draw_vline(g, cx, y0 + 1, ymid, 255);
        glyph_draw_vline(g, cx, ymid, y1 - 1, 255);
        break;
    case '=':
        glyph_draw_hline(g, x0, x1, ymid - 1, 255);
        glyph_draw_hline(g, x0, x1, ymid + 1, 255);
        break;
    case '>':
        glyph_set_pixel(g, x0, y0 + 1, 255);
        glyph_set_pixel(g, cx, ymid, 255);
        glyph_set_pixel(g, x1, ymid, 255);
        glyph_set_pixel(g, x0, y1 - 1, 255);
        glyph_draw_vline(g, cx, y0 + 1, ymid, 255);
        glyph_draw_vline(g, cx, ymid, y1 - 1, 255);
        break;
    case '?':
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x1, y0, ymid, 255);
        glyph_draw_hline(g, cx, x1, ymid, 255);
        glyph_draw_vline(g, cx, ymid, ymid + 2, 255);
        glyph_set_pixel(g, cx, y1, 255);
        break;
    case '@':
        glyph_draw_rect(g, x0, y0, x1, y1, 255);
        glyph_draw_hline(g, cx, x1 - 1, ymid, 255);
        glyph_draw_vline(g, x1 - 1, y0 + 1, ymid, 255);
        glyph_draw_vline(g, cx, y0 + 1, ymid, 255);
        break;
    case '[':
        glyph_draw_hline(g, cx - 1, cx + 1, y0, 255);
        glyph_draw_vline(g, cx - 1, y0, y1, 255);
        glyph_draw_hline(g, cx - 1, cx + 1, y1, 255);
        break;
    case '\\':
        for (int i = 0; i <= y1 - y0; i++) {
            int dx = (i * (x1 - x0)) / std::max(1, y1 - y0);
            glyph_set_pixel(g, x0 + dx, y0 + i, 255);
        }
        break;
    case ']':
        glyph_draw_hline(g, cx - 1, cx + 1, y0, 255);
        glyph_draw_vline(g, cx + 1, y0, y1, 255);
        glyph_draw_hline(g, cx - 1, cx + 1, y1, 255);
        break;
    case '^':
        glyph_set_pixel(g, cx, y0, 255);
        glyph_set_pixel(g, cx - 1, y0 + 1, 255);
        glyph_set_pixel(g, cx + 1, y0 + 1, 255);
        break;
    case '_':
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case '`':
        glyph_set_pixel(g, cx, y0, 255);
        glyph_set_pixel(g, cx + 1, y0 + 1, 255);
        break;
    case '{':
        glyph_set_pixel(g, cx + 1, y0, 255);
        glyph_draw_vline(g, cx, y0 + 1, ymid - 1, 255);
        glyph_set_pixel(g, cx - 1, ymid, 255);
        glyph_draw_vline(g, cx, ymid + 1, y1 - 1, 255);
        glyph_set_pixel(g, cx + 1, y1, 255);
        break;
    case '|':
        glyph_draw_vline(g, cx, y0, y1, 255);
        break;
    case '}':
        glyph_set_pixel(g, cx - 1, y0, 255);
        glyph_draw_vline(g, cx, y0 + 1, ymid - 1, 255);
        glyph_set_pixel(g, cx + 1, ymid, 255);
        glyph_draw_vline(g, cx, ymid + 1, y1 - 1, 255);
        glyph_set_pixel(g, cx - 1, y1, 255);
        break;
    case '~':
        glyph_set_pixel(g, x0, ymid, 255);
        glyph_set_pixel(g, x0 + 1, ymid - 1, 255);
        glyph_set_pixel(g, cx, ymid, 255);
        glyph_set_pixel(g, x1 - 1, ymid + 1, 255);
        glyph_set_pixel(g, x1, ymid, 255);
        break;
    }
}

void BitmapFont::generate_glyph_latin1(Glyph& g, int codepoint, int w, int h) {
    int x0 = 1, x1 = w - 2;
    int y0 = 1, ymid = h / 2, y1 = h - 2;
    int cx = w / 2;

    // Latin-1 supplement characters (160–255)
    // We generate simplified versions of common characters
    switch (codepoint) {
    case 0xA0: // Non-breaking space
        break;
    case 0xA1: // Inverted exclamation ¡
        glyph_set_pixel(g, cx, y0, 255);
        glyph_draw_vline(g, cx, y0 + 2, y1, 255);
        break;
    case 0xA3: // Pound £
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x0, y0, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 0xA5: // Yen ¥
        glyph_draw_vline(g, x0, y0, ymid - 1, 255);
        glyph_draw_vline(g, x1, y0, ymid - 1, 255);
        glyph_draw_hline(g, x0, x1, ymid - 1, 255);
        glyph_draw_hline(g, x0, x1, ymid + 1, 255);
        glyph_draw_vline(g, cx, ymid - 1, y1, 255);
        break;
    case 0xA9: // Copyright ©
        glyph_draw_rect(g, x0, y0, x1, y1, 255);
        glyph_draw_hline(g, cx - 1, x1 - 1, y0 + 2, 255);
        glyph_draw_vline(g, cx - 1, y0 + 2, y1 - 2, 255);
        glyph_draw_hline(g, cx - 1, x1 - 1, y1 - 2, 255);
        break;
    case 0xAE: // Registered ®
        glyph_draw_rect(g, x0, y0, x1, y1, 255);
        glyph_draw_vline(g, cx - 1, y0 + 2, y1 - 2, 255);
        glyph_draw_hline(g, cx - 1, x1 - 1, y0 + 2, 255);
        glyph_draw_vline(g, x1 - 1, y0 + 2, ymid, 255);
        glyph_draw_hline(g, cx - 1, x1 - 1, ymid, 255);
        break;
    case 0xB0: // Degree °
        glyph_draw_hline(g, cx - 1, cx + 1, y0, 255);
        glyph_draw_vline(g, cx - 1, y0, y0 + 2, 255);
        glyph_draw_vline(g, cx + 1, y0, y0 + 2, 255);
        glyph_draw_hline(g, cx - 1, cx + 1, y0 + 2, 255);
        break;
    case 0xB1: // Plus-minus ±
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_draw_vline(g, cx, ymid - 2, ymid + 2, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 0xBF: // Inverted question ¿
        glyph_set_pixel(g, cx, y0, 255);
        glyph_draw_vline(g, cx, y0 + 2, ymid, 255);
        glyph_draw_hline(g, x0, cx, ymid, 255);
        glyph_draw_vline(g, x0, ymid, y1, 255);
        glyph_draw_hline(g, x0, x1, y1, 255);
        break;
    case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: // À-Å
        // Draw A with accent marks
        glyph_draw_hline(g, x0, x1, y0 + 2, 255);
        glyph_draw_vline(g, x0, y0 + 2, y1, 255);
        glyph_draw_vline(g, x1, y0 + 2, y1, 255);
        glyph_draw_hline(g, x0, x1, ymid, 255);
        if (codepoint == 0xC0) glyph_set_pixel(g, cx - 1, y0, 255);      // grave
        else if (codepoint == 0xC1) glyph_set_pixel(g, cx + 1, y0, 255); // acute
        else if (codepoint == 0xC2) {                                     // circumflex
            glyph_set_pixel(g, cx, y0, 255);
            glyph_set_pixel(g, cx - 1, y0 + 1, 255);
            glyph_set_pixel(g, cx + 1, y0 + 1, 255);
        }
        else if (codepoint == 0xC3) {                                     // tilde
            glyph_set_pixel(g, cx - 1, y0, 255);
            glyph_set_pixel(g, cx, y0 + 1, 255);
            glyph_set_pixel(g, cx + 1, y0, 255);
        }
        else if (codepoint == 0xC4) {                                     // diaeresis
            glyph_set_pixel(g, cx - 1, y0, 255);
            glyph_set_pixel(g, cx + 1, y0, 255);
        }
        else if (codepoint == 0xC5) {                                     // ring
            glyph_set_pixel(g, cx, y0, 255);
            glyph_set_pixel(g, cx - 1, y0 + 1, 255);
            glyph_set_pixel(g, cx + 1, y0 + 1, 255);
        }
        break;
    case 0xC7: // Ç
        glyph_draw_hline(g, x0, x1, y0, 255);
        glyph_draw_vline(g, x0, y0, y1 - 1, 255);
        glyph_draw_hline(g, x0, x1, y1 - 1, 255);
        glyph_set_pixel(g, cx, y1, 255);
        break;
    case 0xD1: // Ñ
        glyph_draw_vline(g, x0, y0 + 2, y1, 255);
        glyph_draw_vline(g, x1, y0 + 2, y1, 255);
        for (int i = 0; i <= y1 - y0 - 2; i++) {
            int dx = (i * (x1 - x0)) / std::max(1, y1 - y0 - 2);
            glyph_set_pixel(g, x0 + dx, y0 + 2 + i, 255);
        }
        glyph_set_pixel(g, cx - 1, y0, 255);
        glyph_set_pixel(g, cx, y0 + 1, 255);
        glyph_set_pixel(g, cx + 1, y0, 255);
        break;
    case 0xD7: // Multiplication sign ×
        for (int i = 0; i <= y1 - y0; i++) {
            int dx = (i * (x1 - x0)) / std::max(1, y1 - y0);
            glyph_set_pixel(g, x0 + dx, y0 + i, 255);
            glyph_set_pixel(g, x1 - dx, y0 + i, 255);
        }
        break;
    case 0xF7: // Division sign ÷
        glyph_draw_hline(g, x0, x1, ymid, 255);
        glyph_set_pixel(g, cx, ymid - 2, 255);
        glyph_set_pixel(g, cx, ymid + 2, 255);
        break;
    default:
        // For unrecognized Latin-1 characters, draw a filled rectangle placeholder
        glyph_draw_rect(g, x0, y0, x1, y1, 255);
        glyph_set_pixel(g, cx, ymid, 255);
        break;
    }
}

BitmapFont* BitmapFont::generate_builtin(int requested_size) {
    // Choose cell dimensions based on requested size
    int cell_w, cell_h;
    if (requested_size <= 10) {
        cell_w = 6;
        cell_h = 10;
    } else {
        cell_w = 8;
        cell_h = 16;
    }

    BitmapFont* font = new BitmapFont();
    font->name = "PixelForge Built-in";
    font->size = cell_h;
    font->is_monospace = true;

    font->metrics.ascent = cell_h - 2;
    font->metrics.descent = -2;
    font->metrics.line_height = cell_h;
    font->metrics.max_advance = cell_w;
    font->metrics.em_size = cell_h;
    font->metrics.underline_pos = cell_h - 1;
    font->metrics.underline_thickness = 1;

    // Generate ASCII printable range: 32-126

    // Space (32)
    {
        Glyph g = make_blank_glyph(32, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Punctuation 33-47: ! " # $ % & ' ( ) * + , - . /
    for (int cp = 33; cp <= 47; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_punctuation(g, cp, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Digits 48-57: 0-9
    for (int cp = 48; cp <= 57; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_digit(g, cp - 48, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Punctuation 58-64: : ; < = > ? @
    for (int cp = 58; cp <= 64; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_punctuation(g, cp, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Uppercase letters 65-90: A-Z
    for (int cp = 65; cp <= 90; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_letter_upper(g, cp - 65, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Punctuation 91-96: [ \ ] ^ _ `
    for (int cp = 91; cp <= 96; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_punctuation(g, cp, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Lowercase letters 97-122: a-z
    for (int cp = 97; cp <= 122; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_letter_lower(g, cp - 97, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Punctuation 123-126: { | } ~
    for (int cp = 123; cp <= 126; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_punctuation(g, cp, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    // Latin-1 supplement: 160-255 — commonly used accented characters
    for (int cp = 160; cp <= 255; cp++) {
        Glyph g = make_blank_glyph(cp, cell_w, cell_h);
        generate_glyph_latin1(g, cp, cell_w, cell_h);
        font->add_glyph(std::move(g));
    }

    return font;
}

} // namespace PixelForge
