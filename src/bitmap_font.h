#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "image.h"
#include "errors.h"

namespace PixelForge {

// Represents a single rendered glyph in a bitmap font
struct Glyph {
    int codepoint = 0;          // Unicode codepoint this glyph represents
    int width = 0;              // Width of the glyph bitmap in pixels
    int height = 0;             // Height of the glyph bitmap in pixels
    int advance_x = 0;          // Horizontal advance after rendering this glyph
    int advance_y = 0;          // Vertical advance (usually 0 for horizontal text)
    int bearing_x = 0;          // X offset from cursor to top-left of bitmap
    int bearing_y = 0;          // Y offset from baseline to top of bitmap
    std::vector<uint8_t> bitmap; // 8-bit alpha bitmap, row-major, width*height bytes

    // Returns true if this glyph has a valid bitmap
    bool has_bitmap() const { return !bitmap.empty() && width > 0 && height > 0; }

    // Get the alpha value at a specific pixel in the glyph bitmap
    uint8_t get_alpha(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0;
        return bitmap[static_cast<size_t>(y * width + x)];
    }

    // Returns the total number of bytes in the bitmap
    size_t bitmap_size() const { return static_cast<size_t>(width) * static_cast<size_t>(height); }
};

// Aggregate metrics for the entire font face
struct FontMetrics {
    int ascent = 0;        // Distance from baseline to top of tallest glyph
    int descent = 0;       // Distance from baseline to bottom of lowest glyph (usually negative)
    int line_height = 0;   // Recommended vertical distance between baselines
    int max_advance = 0;   // Maximum horizontal advance across all glyphs
    int em_size = 0;       // The em square size
    int underline_pos = 0; // Position of underline relative to baseline
    int underline_thickness = 1; // Thickness of underline in pixels
};

// PSF2 file format header structure (PC Screen Font version 2)
struct PSF2Header {
    uint32_t magic;            // Magic bytes: 0x864ab572
    uint32_t version;          // Zero
    uint32_t header_size;      // Offset of bitmaps in file, 32
    uint32_t flags;            // 0 if no unicode table, 1 if unicode table
    uint32_t glyph_count;      // Number of glyphs
    uint32_t bytes_per_glyph;  // Size of each glyph in bytes
    uint32_t height;           // Height in pixels
    uint32_t width;            // Width in pixels
};

// Bitmap font class supporting multiple font formats and built-in generation
class BitmapFont {
public:
    BitmapFont();
    ~BitmapFont();

    // Font identification
    std::string name;
    int size = 0;

    // Font metrics
    FontMetrics metrics;

    // Glyph storage indexed by Unicode codepoint
    std::unordered_map<int, Glyph> glyphs;

    // Glyph access
    const Glyph* get_glyph(int codepoint) const;
    Glyph* get_glyph_mut(int codepoint);
    bool has_glyph(int codepoint) const;
    const FontMetrics& get_metrics() const;

    // Add a glyph to the font
    void add_glyph(const Glyph& glyph);
    void add_glyph(Glyph&& glyph);

    // Get the number of glyphs in this font
    size_t glyph_count() const { return glyphs.size(); }

    // Text measurement
    int text_width(const std::string& text) const;
    int text_height(const std::string& text) const;
    void text_extents(const std::string& text, int* out_width, int* out_height) const;

    // Kerning (simple pair-based)
    void set_kerning(int left_cp, int right_cp, int kern_x);
    int get_kerning(int left_cp, int right_cp) const;

    // Glyph atlas generation — packs all glyphs into a single image
    Image* generate_atlas(int* atlas_cols, int* atlas_rows) const;

    // Static factory loaders
    static BitmapFont* load_psf2(const uint8_t* data, size_t size);
    static BitmapFont* load_bdf(const uint8_t* data, size_t size);
    static BitmapFont* generate_builtin(int size);

    // Monospaced flag
    bool is_monospace = false;

private:
    // Kerning pairs: key = (left_codepoint << 16) | right_codepoint
    std::unordered_map<uint32_t, int> m_kerning;

    // UTF-8 decoding helper: returns codepoint and advances pos
    static int decode_utf8(const std::string& text, size_t& pos);

    // Internal glyph bitmap generators for the built-in font
    static void generate_glyph_digit(Glyph& g, int digit, int cell_w, int cell_h);
    static void generate_glyph_letter_upper(Glyph& g, int letter_index, int cell_w, int cell_h);
    static void generate_glyph_letter_lower(Glyph& g, int letter_index, int cell_w, int cell_h);
    static void generate_glyph_punctuation(Glyph& g, int codepoint, int cell_w, int cell_h);
    static void generate_glyph_latin1(Glyph& g, int codepoint, int cell_w, int cell_h);

    // Helper to set pixel in a glyph bitmap under construction
    static void glyph_set_pixel(Glyph& g, int x, int y, uint8_t val);
    static void glyph_draw_hline(Glyph& g, int x0, int x1, int y, uint8_t val);
    static void glyph_draw_vline(Glyph& g, int x, int y0, int y1, uint8_t val);
    static void glyph_draw_rect(Glyph& g, int x0, int y0, int x1, int y1, uint8_t val);
    static void glyph_fill_rect(Glyph& g, int x0, int y0, int x1, int y1, uint8_t val);
};

} // namespace PixelForge
