#pragma once

#include "image.h"
#include "errors.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace PixelForge {

// ============================================================================
// PaletteEntry — a single color in the palette
// ============================================================================

struct PaletteEntry {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
    float frequency = 0.0f;  // usage frequency in the source image [0..1]

    PaletteEntry() = default;
    PaletteEntry(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_), frequency(0.0f) {}
};

// ============================================================================
// Palette — a collection of up to 256 colors
// ============================================================================

class Palette {
public:
    static constexpr size_t MAX_ENTRIES = 256;

    Palette();
    ~Palette() = default;

    // Add a color to the palette; returns false if palette is full
    bool add_color(const PaletteEntry& entry);
    bool add_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    // Get entry count
    size_t size() const { return m_entries.size(); }
    bool empty() const { return m_entries.empty(); }

    // Access entries
    const PaletteEntry& operator[](size_t index) const { return m_entries[index]; }
    PaletteEntry& operator[](size_t index) { return m_entries[index]; }
    const std::vector<PaletteEntry>& entries() const { return m_entries; }

    // Find the nearest color in the palette to the given pixel (squared Euclidean)
    // Returns the index of the nearest entry
    size_t find_nearest(uint8_t r, uint8_t g, uint8_t b) const;
    size_t find_nearest(const Pixel& pixel) const;

    // Sort the palette by perceived luminance (dark → light)
    void sort_by_luminance();

    // Sort the palette by frequency (most frequent first)
    void sort_by_frequency();

    // Reduce the palette to at most target_count colors by merging similar colors
    void reduce(size_t target_count);

    // Remove duplicate entries
    void deduplicate();

    // Clear the palette
    void clear();

    // ========================================================================
    // Quantization algorithms — build a palette from an image
    // ========================================================================

    // Median Cut quantization (Heckbert 1982)
    static Palette quantize_median_cut(const Image& img, int max_colors);

    // Octree quantization
    static Palette quantize_octree(const Image& img, int max_colors);

    // ========================================================================
    // Dithering — apply palette to an image with error diffusion
    // ========================================================================

    // Ordered dithering using a Bayer matrix (2×2, 4×4, or 8×8)
    static PixelForgeErrorCode apply_ordered_dither(
        Image& img, const Palette& pal, int matrix_size);

    // Floyd-Steinberg error diffusion with serpentine scanning
    static PixelForgeErrorCode apply_floyd_steinberg(
        Image& img, const Palette& pal);

    // ========================================================================
    // Palette I/O
    // ========================================================================

    // Save palette in GIMP .gpl format
    PixelForgeErrorCode save_gpl(const std::string& filename,
                                  const std::string& palette_name = "PixelForge") const;

    // Load palette from GIMP .gpl file
    static Palette load_gpl(const std::string& filename, PixelForgeErrorCode* err = nullptr);

    // Save palette in Adobe .aco format (version 1)
    PixelForgeErrorCode save_aco(const std::string& filename) const;

    // Load palette from Adobe .aco file
    static Palette load_aco(const std::string& filename, PixelForgeErrorCode* err = nullptr);

private:
    std::vector<PaletteEntry> m_entries;
};

} // namespace PixelForge
