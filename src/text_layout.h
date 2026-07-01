#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>
#include <memory>
#include "bitmap_font.h"
#include "image.h"

namespace PixelForge {

// ─────────────────────────────────────────────────────────────────────────────
// Text alignment within a layout box
// ─────────────────────────────────────────────────────────────────────────────
enum class TextAlign {
    Left,       // Left-aligned (default)
    Center,     // Centered horizontally
    Right,      // Right-aligned
    Justify     // Justified — extra space distributed between words
};

// ─────────────────────────────────────────────────────────────────────────────
// Text wrapping mode
// ─────────────────────────────────────────────────────────────────────────────
enum class TextWrap {
    None,       // No wrapping — single line, may overflow
    Word,       // Wrap at word boundaries (whitespace)
    Character   // Wrap at character boundaries (break mid-word if needed)
};

// ─────────────────────────────────────────────────────────────────────────────
// Text style applied to a run of text
// ─────────────────────────────────────────────────────────────────────────────
struct TextStyle {
    BitmapFont* font = nullptr;    // The bitmap font to use
    uint8_t color_r = 255;         // Text color red component
    uint8_t color_g = 255;         // Text color green component
    uint8_t color_b = 255;         // Text color blue component
    uint8_t color_a = 255;         // Text color alpha component
    float opacity = 1.0f;          // Overall opacity multiplier [0.0, 1.0]
    TextAlign align = TextAlign::Left;
    TextWrap wrap = TextWrap::Word;
    int line_spacing = 0;          // Additional pixels between lines (beyond font line_height)
    bool underline = false;        // Draw underline
    bool strikethrough = false;    // Draw strikethrough line
    int letter_spacing = 0;        // Extra pixels between characters
};

// ─────────────────────────────────────────────────────────────────────────────
// A run of text with uniform style
// ─────────────────────────────────────────────────────────────────────────────
struct TextRun {
    std::string text;
    TextStyle style;
};

// ─────────────────────────────────────────────────────────────────────────────
// A positioned glyph within a layout line
// ─────────────────────────────────────────────────────────────────────────────
struct PositionedGlyph {
    const Glyph* glyph = nullptr;  // Pointer into font's glyph map
    int x = 0;                     // X position relative to line origin
    int y = 0;                     // Y position relative to line baseline
    uint8_t color_r = 255;
    uint8_t color_g = 255;
    uint8_t color_b = 255;
    uint8_t color_a = 255;
    float opacity = 1.0f;
    bool is_whitespace = false;    // Whether this is a space character
};

// ─────────────────────────────────────────────────────────────────────────────
// A single line of laid-out text
// ─────────────────────────────────────────────────────────────────────────────
struct LayoutLine {
    std::vector<PositionedGlyph> positioned_glyphs;
    int y = 0;                     // Y position of the line's baseline
    int width = 0;                 // Total width of content on this line
    int height = 0;                // Height of the line
    int ascent = 0;                // Maximum ascent on this line
    int descent = 0;               // Maximum descent on this line
    int word_count = 0;            // Number of word boundaries (for justify)
};

// ─────────────────────────────────────────────────────────────────────────────
// Word token used during layout computation
// ─────────────────────────────────────────────────────────────────────────────
struct WordToken {
    std::string text;
    int width = 0;                 // Pixel width of the word
    int space_width = 0;           // Width of trailing space(s)
    TextStyle style;
    bool ends_with_newline = false;
    std::vector<std::pair<int, const Glyph*>> char_glyphs; // codepoint + glyph pairs
};

// ─────────────────────────────────────────────────────────────────────────────
// Text layout engine
// ─────────────────────────────────────────────────────────────────────────────
class TextLayout {
public:
    TextLayout(int max_width, int max_height);
    ~TextLayout() = default;

    // Add a styled text run
    void add_run(const TextRun& run);
    void add_run(TextRun&& run);

    // Clear all runs
    void clear();

    // Compute the layout — call after adding all runs
    std::vector<LayoutLine> compute_layout();

    // Render the laid-out text onto a target image
    void render(Image& target, int x, int y);

    // Get the computed bounding box after layout
    int get_computed_width() const { return m_computed_width; }
    int get_computed_height() const { return m_computed_height; }

    // Settings
    void set_max_width(int w) { m_max_width = w; }
    void set_max_height(int h) { m_max_height = h; }
    void set_default_align(TextAlign align) { m_default_align = align; }
    void set_default_wrap(TextWrap wrap) { m_default_wrap = wrap; }

private:
    int m_max_width;
    int m_max_height;
    TextAlign m_default_align = TextAlign::Left;
    TextWrap m_default_wrap = TextWrap::Word;
    int m_computed_width = 0;
    int m_computed_height = 0;

    std::vector<TextRun> m_runs;
    std::vector<LayoutLine> m_lines;

    // Internal layout helpers
    std::vector<WordToken> tokenize_runs();
    void break_into_lines(const std::vector<WordToken>& words);
    void apply_alignment();
    void apply_justification(LayoutLine& line, int target_width);

    // Glyph blitting
    void blit_glyph(Image& target, const Glyph* glyph, int x, int y,
                    uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca, float opacity);

    // UTF-8 helpers
    static int decode_utf8(const std::string& text, size_t& pos);
};

} // namespace PixelForge
