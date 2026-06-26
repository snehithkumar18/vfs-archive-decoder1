#ifndef PIXELFORGE_EFFECTS_H
#define PIXELFORGE_EFFECTS_H

#include "image.h"
#include "errors.h"

namespace PixelForge {

enum class ColorBlindType {
    PROTANOPIA = 0,    // Red-insensitive
    DEUTERANOPIA,     // Green-insensitive
    TRITANOPIA        // Blue-insensitive
};

class Effects {
public:
    // Brightness and Contrast
    static PixelForgeErrorCode AdjustBrightnessContrast(const Image& src, Image& dst, float brightness, float contrast);

    // Gamma correction
    static PixelForgeErrorCode GammaCorrection(const Image& src, Image& dst, float gamma);

    // Sepia tone
    static PixelForgeErrorCode Sepia(const Image& src, Image& dst);

    // Color grading via Lookup Table (LUT)
    static PixelForgeErrorCode ApplyLUT(const Image& src, Image& dst, const Image& lut);

    // Vignette effect (darkens image corners)
    static PixelForgeErrorCode Vignette(const Image& src, Image& dst, float intensity, float falloff);

    // Color blindness simulator
    static PixelForgeErrorCode SimulateColorBlindness(const Image& src, Image& dst, ColorBlindType type);
};

} // namespace PixelForge

#endif // PIXELFORGE_EFFECTS_H
