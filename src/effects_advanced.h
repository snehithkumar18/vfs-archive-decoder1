#ifndef PIXELFORGE_EFFECTS_ADVANCED_H
#define PIXELFORGE_EFFECTS_ADVANCED_H

#include "image.h"
#include "errors.h"
#include <string>

namespace PixelForge {

enum class DitherPalette {
    MONOCHROME = 0, // Black and white (1-bit)
    CGA,            // 4-color CGA palette
    EGA,            // 16-color EGA palette
    WEB_SAFE        // 216-color web-safe palette
};

enum class HalftonePattern {
    DOT = 0,
    LINE,
    CROSS
};

struct ShadowParams {
    int32_t offsetX{5};
    int32_t offsetY{5};
    float blurRadius{4.0f};
    float opacity{0.5f};
    uint8_t shadowColor[3]{0, 0, 0};
};

class AdvancedEffects {
public:
    // Floyd-Steinberg Dithering to reduce colors with error diffusion
    static PixelForgeErrorCode FloydSteinbergDither(const Image& src, Image& dst, DitherPalette palette);

    // Halftoning effect
    static PixelForgeErrorCode Halftone(const Image& src, Image& dst, HalftonePattern pattern, uint32_t dotSize, float angle);

    // Chromatic Aberration (Lens color separation shift)
    static PixelForgeErrorCode ChromaticAberration(const Image& src, Image& dst, int32_t shiftX, int32_t shiftY);

    // Drop Shadow generator (creates an expanded canvas with a drop shadow behind the source image)
    static PixelForgeErrorCode DropShadow(const Image& src, Image& dst, const ShadowParams& params);

    // Solarize effect (inverts colors above a threshold intensity)
    static PixelForgeErrorCode Solarize(const Image& src, Image& dst, uint8_t threshold);

    // Posterize effect (bins continuous color tones into discrete levels)
    static PixelForgeErrorCode Posterize(const Image& src, Image& dst, uint32_t levels);

    // Thermal Vision Simulation (colors mapped to a heat ramp)
    static PixelForgeErrorCode ThermalVision(const Image& src, Image& dst);

    // Oil Painting (local histogram rank filter effect)
    static PixelForgeErrorCode OilPainting(const Image& src, Image& dst, uint32_t radius, uint32_t levels);

    // Pixelate (averages pixels in grid blocks)
    static PixelForgeErrorCode Pixelate(const Image& src, Image& dst, uint32_t blockSize);

    // Glitch effect (horizontal strip shifting and channel split)
    static PixelForgeErrorCode Glitch(const Image& src, Image& dst, uint32_t stripHeight, int32_t maxShift, uint32_t seed);

    // Anaglyph 3D (combines red channel of left image and green/blue channels of right image)
    static PixelForgeErrorCode Anaglyph3D(const Image& left, const Image& right, Image& dst);

    // ASCII Art Generator (renders ASCII representation of image back into a new monochrome image)
    static PixelForgeErrorCode ASCIIArt(const Image& src, Image& dst, uint32_t charWidth, uint32_t charHeight);

    // Kaleidoscope effect (mirrors 1/8th of the image across the entire canvas)
    static PixelForgeErrorCode Kaleidoscope(const Image& src, Image& dst, uint32_t centersCount);
};

} // namespace PixelForge

#endif // PIXELFORGE_EFFECTS_ADVANCED_H
