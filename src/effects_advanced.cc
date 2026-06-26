#include "effects_advanced.h"
#include "logger.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>

namespace PixelForge {

// Helper to find the closest color in the selected palette
static void findClosestPaletteColor(uint8_t r, uint8_t g, uint8_t b, DitherPalette palette,
                                    uint8_t& outR, uint8_t& outG, uint8_t& outB) {
    if (palette == DitherPalette::MONOCHROME) {
        // Monochrome (1-bit) - threshold based on simple luminance
        uint32_t lum = static_cast<uint32_t>(0.299f * r + 0.587f * g + 0.114f * b);
        if (lum < 128) {
            outR = 0; outG = 0; outB = 0;
        } else {
            outR = 255; outG = 255; outB = 255;
        }
    } else if (palette == DitherPalette::CGA) {
        // CGA 4-color palette (Black, Cyan, Magenta, White)
        struct Color { uint8_t r, g, b; };
        static const Color cgaColors[4] = {
            {0, 0, 0},       // Black
            {0, 255, 255},   // Cyan
            {255, 0, 255},   // Magenta
            {255, 255, 255}  // White
        };
        uint32_t minDist = 0xFFFFFFFF;
        size_t bestIdx = 0;
        for (size_t i = 0; i < 4; ++i) {
            int32_t dr = r - cgaColors[i].r;
            int32_t dg = g - cgaColors[i].g;
            int32_t db = b - cgaColors[i].b;
            uint32_t dist = dr * dr + dg * dg + db * db;
            if (dist < minDist) {
                minDist = dist;
                bestIdx = i;
            }
        }
        outR = cgaColors[bestIdx].r;
        outG = cgaColors[bestIdx].g;
        outB = cgaColors[bestIdx].b;
    } else if (palette == DitherPalette::EGA) {
        // EGA 16-color palette
        struct Color { uint8_t r, g, b; };
        static const Color egaColors[16] = {
            {0, 0, 0},       {0, 0, 170},     {0, 170, 0},     {0, 170, 170},
            {170, 0, 0},     {170, 0, 170},   {170, 85, 0},    {170, 170, 170},
            {85, 85, 85},    {85, 85, 255},   {85, 255, 85},   {85, 255, 255},
            {255, 85, 85},   {255, 85, 255},  {255, 255, 85},  {255, 255, 255}
        };
        uint32_t minDist = 0xFFFFFFFF;
        size_t bestIdx = 0;
        for (size_t i = 0; i < 16; ++i) {
            int32_t dr = r - egaColors[i].r;
            int32_t dg = g - egaColors[i].g;
            int32_t db = b - egaColors[i].b;
            uint32_t dist = dr * dr + dg * dg + db * db;
            if (dist < minDist) {
                minDist = dist;
                bestIdx = i;
            }
        }
        outR = egaColors[bestIdx].r;
        outG = egaColors[bestIdx].g;
        outB = egaColors[bestIdx].b;
    } else {
        // WEB_SAFE (216 colors)
        // Red, Green, Blue components are binned to multiples of 51 (i.e. 0, 51, 102, 153, 204, 255)
        outR = static_cast<uint8_t>(std::round(r / 51.0f) * 51.0f);
        outG = static_cast<uint8_t>(std::round(g / 51.0f) * 51.0f);
        outB = static_cast<uint8_t>(std::round(b / 51.0f) * 51.0f);
    }
}

PixelForgeErrorCode AdvancedEffects::FloydSteinbergDither(const Image& src, Image& dst, DitherPalette palette) {
    if (!src.isValid()) {
        Logger::getInstance().error("Dither: Invalid source image.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    if (ch < 3) {
        Logger::getInstance().error("Dither: Requires an RGB/RGBA image.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    // Use floating-point error buffers to prevent accumulation clamping errors
    // Extra padding around the error buffer to avoid boundary check branches inside loops
    std::vector<float> rErrBuf((w + 2) * (h + 2), 0.0f);
    std::vector<float> gErrBuf((w + 2) * (h + 2), 0.0f);
    std::vector<float> bErrBuf((w + 2) * (h + 2), 0.0f);

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    // Populate initial errors with source pixel values
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t srcIdx = (y * w + x) * ch;
            size_t errIdx = (y + 1) * (w + 2) + (x + 1);
            rErrBuf[errIdx] = srcData[srcIdx + 0];
            gErrBuf[errIdx] = srcData[srcIdx + 1];
            bErrBuf[errIdx] = srcData[srcIdx + 2];
        }
    }

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t errIdx = (y + 1) * (w + 2) + (x + 1);
            float currR = rErrBuf[errIdx];
            float currG = gErrBuf[errIdx];
            float currB = bErrBuf[errIdx];

            // Clamp accumulated floating values to standard [0, 255] range
            uint8_t r = static_cast<uint8_t>(std::clamp(currR, 0.0f, 255.0f));
            uint8_t g = static_cast<uint8_t>(std::clamp(currG, 0.0f, 255.0f));
            uint8_t b = static_cast<uint8_t>(std::clamp(currB, 0.0f, 255.0f));

            uint8_t closestR, closestG, closestB;
            findClosestPaletteColor(r, g, b, palette, closestR, closestG, closestB);

            // Write closest palette color to destination image
            size_t dstIdx = (y * w + x) * ch;
            dstData[dstIdx + 0] = closestR;
            dstData[dstIdx + 1] = closestG;
            dstData[dstIdx + 2] = closestB;
            if (ch == 4) {
                dstData[dstIdx + 3] = srcData[dstIdx + 3]; // Preserve alpha
            }

            // Error differences
            float errR = currR - closestR;
            float errG = currG - closestG;
            float errB = currB - closestB;

            // Distribute error to neighboring pixels based on Floyd-Steinberg coefficients:
            //       *   7/16
            // 3/16 5/16 1/16
            size_t idxRight = errIdx + 1;
            size_t idxDownLeft = errIdx + (w + 2) - 1;
            size_t idxDown = errIdx + (w + 2);
            size_t idxDownRight = errIdx + (w + 2) + 1;

            rErrBuf[idxRight] += errR * 7.0f / 16.0f;
            gErrBuf[idxRight] += errG * 7.0f / 16.0f;
            bErrBuf[idxRight] += errB * 7.0f / 16.0f;

            rErrBuf[idxDownLeft] += errR * 3.0f / 16.0f;
            gErrBuf[idxDownLeft] += errG * 3.0f / 16.0f;
            bErrBuf[idxDownLeft] += errB * 3.0f / 16.0f;

            rErrBuf[idxDown] += errR * 5.0f / 16.0f;
            gErrBuf[idxDown] += errG * 5.0f / 16.0f;
            bErrBuf[idxDown] += errB * 5.0f / 16.0f;

            rErrBuf[idxDownRight] += errR * 1.0f / 16.0f;
            gErrBuf[idxDownRight] += errG * 1.0f / 16.0f;
            bErrBuf[idxDownRight] += errB * 1.0f / 16.0f;
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::Halftone(const Image& src, Image& dst, HalftonePattern pattern, uint32_t dotSize, float angle) {
    if (!src.isValid() || dotSize == 0) {
        Logger::getInstance().error("Halftone: Invalid parameter.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    // Default fill the destination image with white pixels
    std::fill(dstData.begin(), dstData.end(), 255);

    float angleRad = angle * 3.14159265f / 180.0f;
    float cosA = std::cos(angleRad);
    float sinA = std::sin(angleRad);

    // Grid cells loops
    for (uint32_t cy = 0; cy < h; cy += dotSize) {
        for (uint32_t cx = 0; cx < w; cx += dotSize) {
            
            // Calculate average intensity inside current cell
            uint64_t totalLuminance = 0;
            uint32_t pixelCount = 0;

            for (uint32_t dy = 0; dy < dotSize && (cy + dy) < h; ++dy) {
                for (uint32_t dx = 0; dx < dotSize && (cx + dx) < w; ++dx) {
                    size_t idx = ((cy + dy) * w + (cx + dx)) * ch;
                    uint8_t gray = 0;
                    if (ch >= 3) {
                        gray = static_cast<uint8_t>(0.299f * srcData[idx + 0] + 0.587f * srcData[idx + 1] + 0.114f * srcData[idx + 2]);
                    } else {
                        gray = srcData[idx + 0];
                    }
                    totalLuminance += gray;
                    pixelCount++;
                }
            }

            if (pixelCount == 0) continue;
            float avgLuminance = static_cast<float>(totalLuminance) / pixelCount;
            
            // Normalize average luminance to a scaling factor [0.0, 1.0] where 1.0 is dark (largest dot)
            float darkIntensity = 1.0f - (avgLuminance / 255.0f);
            if (darkIntensity <= 0.05f) continue; // Skip near white cells

            // Cell center coordinates
            float centerX = cx + dotSize / 2.0f;
            float centerY = cy + dotSize / 2.0f;

            // Loop inside cell to draw halftone pattern based on rotation grid
            for (uint32_t dy = 0; dy < dotSize && (cy + dy) < h; ++dy) {
                for (uint32_t dx = 0; dx < dotSize && (cx + dx) < w; ++dx) {
                    uint32_t py = cy + dy;
                    uint32_t px = cx + dx;

                    // Rotate coordinates around cell center
                    float rx = (px - centerX) * cosA - (py - centerY) * sinA;
                    float ry = (px - centerX) * sinA + (py - centerY) * cosA;

                    bool isInsidePattern = false;
                    float maxDist = (dotSize / 2.0f) * darkIntensity;

                    if (pattern == HalftonePattern::DOT) {
                        // Circle dot pattern
                        float dist = std::sqrt(rx * rx + ry * ry);
                        if (dist <= maxDist) {
                            isInsidePattern = true;
                        }
                    } else if (pattern == HalftonePattern::LINE) {
                        // Linear stripe pattern
                        if (std::abs(ry) <= (dotSize / 4.0f) * darkIntensity) {
                            isInsidePattern = true;
                        }
                    } else if (pattern == HalftonePattern::CROSS) {
                        // Cross pattern
                        float threshold = (dotSize / 4.0f) * darkIntensity;
                        if (std::abs(rx) <= threshold || std::abs(ry) <= threshold) {
                            isInsidePattern = true;
                        }
                    }

                    if (isInsidePattern) {
                        size_t dstIdx = (py * w + px) * ch;
                        for (uint32_t c = 0; c < (ch >= 3 ? 3 : 1); ++c) {
                            dstData[dstIdx + c] = 0; // Black dot pixel
                        }
                        if (ch == 4) {
                            dstData[dstIdx + 3] = srcData[dstIdx + 3]; // Preserve alpha
                        }
                    }
                }
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::ChromaticAberration(const Image& src, Image& dst, int32_t shiftX, int32_t shiftY) {
    if (!src.isValid()) {
        Logger::getInstance().error("ChromaticAberration: Invalid source image.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    if (ch < 3) {
        Logger::getInstance().error("ChromaticAberration: Requires RGB/RGBA format.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    for (int32_t y = 0; y < static_cast<int32_t>(h); ++y) {
        for (int32_t x = 0; x < static_cast<int32_t>(w); ++x) {
            size_t dstIdx = (y * w + x) * ch;

            // Red channel shift (positive offset direction)
            int32_t rx = std::clamp(x + shiftX, 0, static_cast<int32_t>(w) - 1);
            int32_t ry = std::clamp(y + shiftY, 0, static_cast<int32_t>(h) - 1);
            size_t redIdx = (ry * w + rx) * ch;

            // Green channel is unshifted
            size_t greenIdx = dstIdx;

            // Blue channel shift (negative offset direction)
            int32_t bx = std::clamp(x - shiftX, 0, static_cast<int32_t>(w) - 1);
            int32_t by = std::clamp(y - shiftY, 0, static_cast<int32_t>(h) - 1);
            size_t blueIdx = (by * w + bx) * ch;

            dstData[dstIdx + 0] = srcData[redIdx + 0];
            dstData[dstIdx + 1] = srcData[greenIdx + 1];
            dstData[dstIdx + 2] = srcData[blueIdx + 2];
            
            if (ch == 4) {
                dstData[dstIdx + 3] = srcData[dstIdx + 3];
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::DropShadow(const Image& src, Image& dst, const ShadowParams& params) {
    if (!src.isValid()) {
        Logger::getInstance().error("DropShadow: Invalid source image.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t srcW = src.getWidth();
    uint32_t srcH = src.getHeight();
    uint32_t ch = src.getChannels();

    // Determine padding due to blur radius
    int32_t pad = static_cast<int32_t>(std::ceil(params.blurRadius * 3.0f));
    if (pad < 0) pad = 0;

    // Output dimension accounts for offset and padding on all sides
    int32_t offsetX = params.offsetX;
    int32_t offsetY = params.offsetY;
    uint32_t dstW = srcW + static_cast<uint32_t>(std::abs(offsetX)) + pad * 2;
    uint32_t dstH = srcH + static_cast<uint32_t>(std::abs(offsetY)) + pad * 2;

    // Shadow must use RGBA output to display shadow alpha transparency
    PixelForgeErrorCode err = dst.allocate(dstW, dstH, PixelFormat::RGBA8888);
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    auto& dstData = dst.getData();
    std::fill(dstData.begin(), dstData.end(), 0); // Fill with fully transparent canvas

    // Target coordinates to place the source image
    // Placing source image at pad offset, while shadow is placed at pad + offset
    int32_t srcX = pad + (offsetX < 0 ? static_cast<int32_t>(std::abs(offsetX)) : 0);
    int32_t srcY = pad + (offsetY < 0 ? static_cast<int32_t>(std::abs(offsetY)) : 0);

    int32_t shadowX = srcX + offsetX;
    int32_t shadowY = srcY + offsetY;

    const auto& srcData = src.getData();

    // 1. Draw solid shadow mask in destination buffer
    for (uint32_t y = 0; y < srcH; ++y) {
        for (uint32_t x = 0; x < srcW; ++x) {
            int32_t sy = shadowY + y;
            int32_t sx = shadowX + x;
            if (sy < 0 || sy >= static_cast<int32_t>(dstH) || sx < 0 || sx >= static_cast<int32_t>(dstW)) {
                continue;
            }

            size_t dstIdx = (sy * dstW + sx) * 4;
            size_t srcIdx = (y * srcW + x) * ch;

            // Extract alpha mask
            float alpha = 1.0f;
            if (ch == 4) {
                alpha = srcData[srcIdx + 3] / 255.0f;
            }

            dstData[dstIdx + 0] = params.shadowColor[0];
            dstData[dstIdx + 1] = params.shadowColor[1];
            dstData[dstIdx + 2] = params.shadowColor[2];
            dstData[dstIdx + 3] = static_cast<uint8_t>(alpha * params.opacity * 255.0f);
        }
    }

    // 2. Apply Box Blur to the shadow channel to simulate soft shadow
    if (params.blurRadius > 0.5f) {
        int32_t radius = static_cast<int32_t>(params.blurRadius);
        std::vector<uint8_t> tempBuffer = dstData;

        // Horizontal pass
        for (uint32_t y = 0; y < dstH; ++y) {
            for (uint32_t x = 0; x < dstW; ++x) {
                uint32_t sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                int32_t count = 0;
                for (int32_t k = -radius; k <= radius; ++k) {
                    int32_t px = static_cast<int32_t>(x) + k;
                    if (px >= 0 && px < static_cast<int32_t>(dstW)) {
                        size_t idx = (y * dstW + px) * 4;
                        sumR += tempBuffer[idx + 0];
                        sumG += tempBuffer[idx + 1];
                        sumB += tempBuffer[idx + 2];
                        sumA += tempBuffer[idx + 3];
                        count++;
                    }
                }
                size_t dstIdx = (y * dstW + x) * 4;
                dstData[dstIdx + 0] = sumR / count;
                dstData[dstIdx + 1] = sumG / count;
                dstData[dstIdx + 2] = sumB / count;
                dstData[dstIdx + 3] = sumA / count;
            }
        }

        // Vertical pass
        tempBuffer = dstData;
        for (uint32_t y = 0; y < dstH; ++y) {
            for (uint32_t x = 0; x < dstW; ++x) {
                uint32_t sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                int32_t count = 0;
                for (int32_t k = -radius; k <= radius; ++k) {
                    int32_t py = static_cast<int32_t>(y) + k;
                    if (py >= 0 && py < static_cast<int32_t>(dstH)) {
                        size_t idx = (py * dstW + x) * 4;
                        sumR += tempBuffer[idx + 0];
                        sumG += tempBuffer[idx + 1];
                        sumB += tempBuffer[idx + 2];
                        sumA += tempBuffer[idx + 3];
                        count++;
                    }
                }
                size_t dstIdx = (y * dstW + x) * 4;
                dstData[dstIdx + 0] = sumR / count;
                dstData[dstIdx + 1] = sumG / count;
                dstData[dstIdx + 2] = sumB / count;
                dstData[dstIdx + 3] = sumA / count;
            }
        }
    }

    // 3. Composite original image over the blurred shadow
    for (uint32_t y = 0; y < srcH; ++y) {
        for (uint32_t x = 0; x < srcW; ++x) {
            int32_t dy = srcY + y;
            int32_t dx = srcX + x;

            if (dy < 0 || dy >= static_cast<int32_t>(dstH) || dx < 0 || dx >= static_cast<int32_t>(dstW)) {
                continue;
            }

            size_t dstIdx = (dy * dstW + dx) * 4;
            size_t srcIdx = (y * srcW + x) * ch;

            // Extract foreground pixel values
            uint8_t fgR = 0, fgG = 0, fgB = 0, fgA = 255;
            if (ch >= 3) {
                fgR = srcData[srcIdx + 0];
                fgG = srcData[srcIdx + 1];
                fgB = srcData[srcIdx + 2];
                if (ch == 4) fgA = srcData[srcIdx + 3];
            } else {
                fgR = fgG = fgB = srcData[srcIdx + 0];
            }

            // Alpha blend over the background shadow pixel
            float srcAlpha = fgA / 255.0f;
            float bgAlpha = dstData[dstIdx + 3] / 255.0f;
            float outAlpha = srcAlpha + bgAlpha * (1.0f - srcAlpha);

            if (outAlpha > 0.0f) {
                dstData[dstIdx + 0] = static_cast<uint8_t>((fgR * srcAlpha + dstData[dstIdx + 0] * bgAlpha * (1.0f - srcAlpha)) / outAlpha);
                dstData[dstIdx + 1] = static_cast<uint8_t>((fgG * srcAlpha + dstData[dstIdx + 1] * bgAlpha * (1.0f - srcAlpha)) / outAlpha);
                dstData[dstIdx + 2] = static_cast<uint8_t>((fgB * srcAlpha + dstData[dstIdx + 2] * bgAlpha * (1.0f - srcAlpha)) / outAlpha);
                dstData[dstIdx + 3] = static_cast<uint8_t>(outAlpha * 255.0f);
            } else {
                dstData[dstIdx + 0] = dstData[dstIdx + 1] = dstData[dstIdx + 2] = dstData[dstIdx + 3] = 0;
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::Solarize(const Image& src, Image& dst, uint8_t threshold) {
    if (!src.isValid()) {
        Logger::getInstance().error("Solarize: Invalid source image.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    for (size_t i = 0; i < srcData.size(); ++i) {
        if (ch == 4 && (i % 4 == 3)) {
            dstData[i] = srcData[i]; // Preserve alpha channel
            continue;
        }

        uint8_t val = srcData[i];
        if (val > threshold) {
            dstData[i] = 255 - val; // Invert color
        } else {
            dstData[i] = val; // Keep color
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::Posterize(const Image& src, Image& dst, uint32_t levels) {
    if (!src.isValid() || levels < 2 || levels > 256) {
        Logger::getInstance().error("Posterize: Invalid parameter levels (must be between 2 and 256).");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    // Map 256 values into levels sections
    uint8_t mapTable[256];
    float levelStep = 255.0f / (levels - 1);
    for (uint32_t i = 0; i < 256; ++i) {
        mapTable[i] = static_cast<uint8_t>(std::clamp(std::round(i / levelStep) * levelStep, 0.0f, 255.0f));
    }

    for (size_t i = 0; i < srcData.size(); ++i) {
        if (ch == 4 && (i % 4 == 3)) {
            dstData[i] = srcData[i];
            continue;
        }
        dstData[i] = mapTable[srcData[i]];
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::ThermalVision(const Image& src, Image& dst) {
    if (!src.isValid()) {
        Logger::getInstance().error("ThermalVision: Invalid source image.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    // Thermal output is always RGB (3 channels) or RGBA (4 channels)
    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    if (ch < 3) {
        Logger::getInstance().error("ThermalVision: Requires RGB or RGBA image.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    for (size_t i = 0; i < srcData.size(); i += ch) {
        uint8_t r = srcData[i + 0];
        uint8_t g = srcData[i + 1];
        uint8_t b = srcData[i + 2];

        // Luminance intensity [0.0 - 1.0]
        float val = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;

        // Thermal color ramp calculation
        float thR = 0.0f;
        float thG = 0.0f;
        float thB = 0.0f;

        if (val < 0.25f) {
            // Blue
            thR = 0.0f;
            thG = 0.0f;
            thB = val * 4.0f;
        } else if (val < 0.5f) {
            // Blue to Cyan/Green
            thR = 0.0f;
            thG = (val - 0.25f) * 4.0f;
            thB = 1.0f;
        } else if (val < 0.75f) {
            // Cyan/Green to Yellow/Orange
            thR = (val - 0.5f) * 4.0f;
            thG = 1.0f;
            thB = 1.0f - (val - 0.5f) * 4.0f;
        } else {
            // Yellow/Orange to Red/White
            thR = 1.0f;
            thG = 1.0f - (val - 0.75f) * 4.0f;
            thB = (val - 0.75f) * 4.0f;
        }

        dstData[i + 0] = static_cast<uint8_t>(std::clamp(thR * 255.0f, 0.0f, 255.0f));
        dstData[i + 1] = static_cast<uint8_t>(std::clamp(thG * 255.0f, 0.0f, 255.0f));
        dstData[i + 2] = static_cast<uint8_t>(std::clamp(thB * 255.0f, 0.0f, 255.0f));

        if (ch == 4) {
            dstData[i + 3] = srcData[i + 3];
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::OilPainting(const Image& src, Image& dst, uint32_t radius, uint32_t levels) {
    if (!src.isValid() || radius == 0 || levels == 0) {
        Logger::getInstance().error("OilPainting: Invalid parameter.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    if (ch < 3) {
        Logger::getInstance().error("OilPainting: Requires RGB/RGBA image.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    // Process every single pixel in the image
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            
            // Temporary structures for local histograms
            std::vector<uint32_t> intensityCount(levels, 0);
            std::vector<uint64_t> sumR(levels, 0);
            std::vector<uint64_t> sumG(levels, 0);
            std::vector<uint64_t> sumB(levels, 0);

            // Neighborhood boundaries
            int32_t minX = std::max(0, static_cast<int32_t>(x) - static_cast<int32_t>(radius));
            int32_t maxX = std::min(static_cast<int32_t>(w) - 1, static_cast<int32_t>(x) + static_cast<int32_t>(radius));
            int32_t minY = std::max(0, static_cast<int32_t>(y) - static_cast<int32_t>(radius));
            int32_t maxY = std::min(static_cast<int32_t>(h) - 1, static_cast<int32_t>(y) + static_cast<int32_t>(radius));

            // Accumulate neighboring pixel intensities
            for (int32_t ny = minY; ny <= maxY; ++ny) {
                for (int32_t nx = minX; nx <= maxX; ++nx) {
                    size_t idx = (ny * w + nx) * ch;
                    uint8_t r = srcData[idx + 0];
                    uint8_t g = srcData[idx + 1];
                    uint8_t b = srcData[idx + 2];

                    float lum = 0.299f * r + 0.587f * g + 0.114f * b;
                    uint32_t intensityIdx = static_cast<uint32_t>((lum / 255.0f) * (levels - 1));
                    
                    if (intensityIdx >= levels) intensityIdx = levels - 1;

                    intensityCount[intensityIdx]++;
                    sumR[intensityIdx] += r;
                    sumG[intensityIdx] += g;
                    sumB[intensityIdx] += b;
                }
            }

            // Find the most frequent intensity in neighborhood
            uint32_t maxCount = 0;
            uint32_t bestIntensityIdx = 0;
            for (uint32_t i = 0; i < levels; ++i) {
                if (intensityCount[i] > maxCount) {
                    maxCount = intensityCount[i];
                    bestIntensityIdx = i;
                }
            }

            // Set destination pixel to the average color of the best intensity bin
            size_t dstIdx = (y * w + x) * ch;
            if (maxCount > 0) {
                dstData[dstIdx + 0] = static_cast<uint8_t>(sumR[bestIntensityIdx] / maxCount);
                dstData[dstIdx + 1] = static_cast<uint8_t>(sumG[bestIntensityIdx] / maxCount);
                dstData[dstIdx + 2] = static_cast<uint8_t>(sumB[bestIntensityIdx] / maxCount);
            } else {
                dstData[dstIdx + 0] = srcData[dstIdx + 0];
                dstData[dstIdx + 1] = srcData[dstIdx + 1];
                dstData[dstIdx + 2] = srcData[dstIdx + 2];
            }

            if (ch == 4) {
                dstData[dstIdx + 3] = srcData[dstIdx + 3];
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::Pixelate(const Image& src, Image& dst, uint32_t blockSize) {
    if (!src.isValid() || blockSize == 0) {
        Logger::getInstance().error("Pixelate: Invalid parameter.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    // Iterate through blocks
    for (uint32_t by = 0; by < h; by += blockSize) {
        for (uint32_t bx = 0; bx < w; bx += blockSize) {
            
            // Calculate actual block boundaries
            uint32_t blockW = std::min(blockSize, w - bx);
            uint32_t blockH = std::min(blockSize, h - by);

            uint64_t sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            uint32_t pixelCount = blockW * blockH;

            // Step 1: Average color in block
            for (uint32_t dy = 0; dy < blockH; ++dy) {
                for (uint32_t dx = 0; dx < blockW; ++dx) {
                    size_t idx = ((by + dy) * w + (bx + dx)) * ch;
                    if (ch >= 3) {
                        sumR += srcData[idx + 0];
                        sumG += srcData[idx + 1];
                        sumB += srcData[idx + 2];
                        if (ch == 4) sumA += srcData[idx + 3];
                    } else {
                        sumR += srcData[idx + 0];
                    }
                }
            }

            uint8_t avgR = static_cast<uint8_t>(sumR / pixelCount);
            uint8_t avgG = static_cast<uint8_t>(sumG / pixelCount);
            uint8_t avgB = static_cast<uint8_t>(sumB / pixelCount);
            uint8_t avgA = static_cast<uint8_t>(sumA / pixelCount);

            // Step 2: Fill destination block with average color
            for (uint32_t dy = 0; dy < blockH; ++dy) {
                for (uint32_t dx = 0; dx < blockW; ++dx) {
                    size_t dstIdx = ((by + dy) * w + (bx + dx)) * ch;
                    if (ch >= 3) {
                        dstData[dstIdx + 0] = avgR;
                        dstData[dstIdx + 1] = avgG;
                        dstData[dstIdx + 2] = avgB;
                        if (ch == 4) dstData[dstIdx + 3] = avgA;
                    } else {
                        dstData[dstIdx + 0] = avgR;
                    }
                }
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::Glitch(const Image& src, Image& dst, uint32_t stripHeight, int32_t maxShift, uint32_t seed) {
    if (!src.isValid() || stripHeight == 0) {
        Logger::getInstance().error("Glitch: Invalid parameter.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    // LCG pseudo-random number generator
    uint32_t state = seed;
    auto nextRand = [&state]() {
        state = state * 1664525 + 1013904223;
        return state;
    };

    for (uint32_t y = 0; y < h; y += stripHeight) {
        uint32_t currStripHeight = stripHeight + (nextRand() % 5);
        if (y + currStripHeight > h) currStripHeight = h - y;

        // Random shift direction and magnitude
        int32_t shift = 0;
        if (nextRand() % 10 < 7) { // 70% chance of being shifted
            shift = (static_cast<int32_t>(nextRand()) % (maxShift * 2)) - maxShift;
        }

        // Channel split toggle (displaces red/blue separately)
        bool splitRGB = (nextRand() % 10 < 3); // 30% chance of RGB split

        for (uint32_t sy = y; sy < y + currStripHeight; ++sy) {
            for (uint32_t sx = 0; sx < w; ++sx) {
                size_t dstIdx = (sy * w + sx) * ch;

                int32_t srcX = static_cast<int32_t>(sx) + shift;
                // Wrap around horizontally
                srcX = (srcX < 0) ? (srcX + static_cast<int32_t>(w)) : (srcX % static_cast<int32_t>(w));

                size_t srcIdx = (sy * w + srcX) * ch;

                if (!splitRGB || ch < 3) {
                    // Copy all channels directly
                    for (uint32_t c = 0; c < ch; ++c) {
                        dstData[dstIdx + c] = srcData[srcIdx + c];
                    }
                } else {
                    // Displace channels separately
                    int32_t rX = srcX + (static_cast<int32_t>(nextRand() % 5) - 2);
                    rX = std::clamp(rX, 0, static_cast<int32_t>(w) - 1);
                    size_t redIdx = (sy * w + rX) * ch;

                    int32_t bX = srcX + (static_cast<int32_t>(nextRand() % 5) - 2);
                    bX = std::clamp(bX, 0, static_cast<int32_t>(w) - 1);
                    size_t blueIdx = (sy * w + bX) * ch;

                    dstData[dstIdx + 0] = srcData[redIdx + 0];   // Displaced red
                    dstData[dstIdx + 1] = srcData[srcIdx + 1];   // Green
                    dstData[dstIdx + 2] = srcData[blueIdx + 2];  // Displaced blue
                    if (ch == 4) {
                        dstData[dstIdx + 3] = srcData[srcIdx + 3];
                    }
                }
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::Anaglyph3D(const Image& left, const Image& right, Image& dst) {
    if (!left.isValid() || !right.isValid()) {
        Logger::getInstance().error("Anaglyph3D: Invalid source image.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    if (left.getWidth() != right.getWidth() || left.getHeight() != right.getHeight()) {
        Logger::getInstance().error("Anaglyph3D: Source images must have identical dimensions.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = left.getWidth();
    uint32_t h = left.getHeight();
    uint32_t leftCh = left.getChannels();
    uint32_t rightCh = right.getChannels();

    if (leftCh < 3 || rightCh < 3) {
        Logger::getInstance().error("Anaglyph3D: Requires RGB/RGBA images.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    PixelForgeErrorCode err = dst.allocate(w, h, left.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& leftData = left.getData();
    const auto& rightData = right.getData();
    auto& dstData = dst.getData();

    uint32_t dstCh = dst.getChannels();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t dstIdx = (y * w + x) * dstCh;
            size_t leftIdx = (y * w + x) * leftCh;
            size_t rightIdx = (y * w + x) * rightCh;

            // Anaglyph 3D uses red channel from left eye and green/blue channels from right eye
            dstData[dstIdx + 0] = leftData[leftIdx + 0];     // Red from Left
            dstData[dstIdx + 1] = rightData[rightIdx + 1];   // Green from Right
            dstData[dstIdx + 2] = rightData[rightIdx + 2];   // Blue from Right
            if (dstCh == 4) {
                dstData[dstIdx + 3] = leftData[leftIdx + 3]; // Preserve Left alpha
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::ASCIIArt(const Image& src, Image& dst, uint32_t charWidth, uint32_t charHeight) {
    if (!src.isValid() || charWidth == 0 || charHeight == 0) {
        Logger::getInstance().error("ASCIIArt: Invalid parameters.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    // Default fill destination with black background
    std::fill(dstData.begin(), dstData.end(), 0);

    // Simplistic monochrome 8x8 font table representing ASCII glyph patterns for levels 0-9
    // 0: space (all empty), 1: '.' (small dot), 2: ':' (double dot), 3: '-' (horizontal bar)
    // 4: '+' (cross), 5: '=' (double bar), 6: 'x' (small cross), 7: 'o' (circle), 8: '8' (thick), 9: '@' (full)
    static const uint8_t asciiGlyphs[10][8] = {
        {0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000000}, // Space
        {0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00011000, 0b00011000, 0b00000000, 0b00000000}, // .
        {0b00000000, 0b00011000, 0b00011000, 0b00000000, 0b00011000, 0b00011000, 0b00000000, 0b00000000}, // :
        {0b00000000, 0b00000000, 0b00000000, 0b01111110, 0b01111110, 0b00000000, 0b00000000, 0b00000000}, // -
        {0b00011000, 0b00011000, 0b00011000, 0b01111110, 0b01111110, 0b00011000, 0b00011000, 0b00011000}, // +
        {0b00000000, 0b01111110, 0b00000000, 0b01111110, 0b00000000, 0b01111110, 0b00000000, 0b00000000}, // =
        {0b01000010, 0b00100100, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00100100, 0b01000010}, // x
        {0b00111100, 0b01000010, 0b01000010, 0b01000010, 0b01000010, 0b01000010, 0b01000010, 0b00111100}, // o
        {0b00111100, 0b01000010, 0b01000010, 0b00111100, 0b00111100, 0b01000010, 0b01000010, 0b00111100}, // 8
        {0b01111110, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b11111111, 0b01111110}  // @
    };

    for (uint32_t cy = 0; cy < h; cy += charHeight) {
        for (uint32_t cx = 0; cx < w; cx += charWidth) {
            
            // 1. Calculate average cell intensity
            uint64_t totalGray = 0;
            uint32_t pixels = 0;

            for (uint32_t dy = 0; dy < charHeight && (cy + dy) < h; ++dy) {
                for (uint32_t dx = 0; dx < charWidth && (cx + dx) < w; ++dx) {
                    size_t idx = ((cy + dy) * w + (cx + dx)) * ch;
                    uint8_t val = 0;
                    if (ch >= 3) {
                        val = static_cast<uint8_t>(0.299f * srcData[idx + 0] + 0.587f * srcData[idx + 1] + 0.114f * srcData[idx + 2]);
                    } else {
                        val = srcData[idx + 0];
                    }
                    totalGray += val;
                    pixels++;
                }
            }

            if (pixels == 0) continue;
            float intensity = static_cast<float>(totalGray) / pixels;

            // Map intensity to one of our 10 levels
            uint32_t glyphIdx = static_cast<uint32_t>((intensity / 255.0f) * 9.0f);
            if (glyphIdx > 9) glyphIdx = 9;

            // 2. Render selected glyph inside output block
            for (uint32_t dy = 0; dy < charHeight && (cy + dy) < h; ++dy) {
                for (uint32_t dx = 0; dx < charWidth && (cx + dx) < w; ++dx) {
                    // Map local dx, dy to 8x8 font grid
                    uint32_t fontX = (dx * 8) / charWidth;
                    uint32_t fontY = (dy * 8) / charHeight;
                    if (fontX >= 8) fontX = 7;
                    if (fontY >= 8) fontY = 7;

                    // Read bit from glyph matrix
                    bool bitActive = (asciiGlyphs[glyphIdx][fontY] & (1 << (7 - fontX))) != 0;

                    size_t dstIdx = ((cy + dy) * w + (cx + dx)) * ch;

                    // Set pixel value: active is white, inactive is black
                    uint8_t colorVal = bitActive ? 255 : 0;
                    for (uint32_t c = 0; c < (ch >= 3 ? 3 : 1); ++c) {
                        dstData[dstIdx + c] = colorVal;
                    }
                    if (ch == 4) {
                        dstData[dstIdx + 3] = 255; // fully opaque character pixels
                    }
                }
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode AdvancedEffects::Kaleidoscope(const Image& src, Image& dst, uint32_t centersCount) {
    if (!src.isValid() || centersCount == 0) {
        Logger::getInstance().error("Kaleidoscope: Invalid parameters.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& srcData = src.getData();
    auto& dstData = dst.getData();

    float centerX = w / 2.0f;
    float centerY = h / 2.0f;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            // Translate relative to center
            float dx = x - centerX;
            float dy = y - centerY;

            // Convert to polar coordinates
            float radius = std::sqrt(dx * dx + dy * dy);
            float angle = std::atan2(dy, dx);

            // Kaleidoscope segment slice angle
            float segmentAngle = 2.0f * 3.14159265f / centersCount;

            // Map angle to a single base segment
            float remainder = std::fmod(angle, segmentAngle);
            if (remainder < 0) remainder += segmentAngle;

            // Mirror reflection inside segment
            if (remainder > segmentAngle / 2.0f) {
                remainder = segmentAngle - remainder;
            }

            // Convert back to cartesian coordinates relative to center
            int32_t srcX = static_cast<int32_t>(centerX + radius * std::cos(remainder));
            int32_t srcY = static_cast<int32_t>(centerY + radius * std::sin(remainder));

            // Clamp coordinate bounds
            srcX = std::clamp(srcX, 0, static_cast<int32_t>(w) - 1);
            srcY = std::clamp(srcY, 0, static_cast<int32_t>(h) - 1);

            size_t dstIdx = (y * w + x) * ch;
            size_t srcIdx = (srcY * w + srcX) * ch;

            for (uint32_t c = 0; c < ch; ++c) {
                dstData[dstIdx + c] = srcData[srcIdx + c];
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge

