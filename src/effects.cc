#include "effects.h"
#include "logger.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace PixelForge {

PixelForgeErrorCode Effects::AdjustBrightnessContrast(const Image& src, Image& dst, float brightness, float contrast) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    // contrast factor
    float factor = (259.0f * (contrast + 255.0f)) / (255.0f * (259.0f - contrast));

    for (size_t i = 0; i < src_data.size(); ++i) {
        if (ch == 4 && (i % 4 == 3)) {
            dst_data[i] = src_data[i];
            continue;
        }

        // Apply brightness
        float val = src_data[i] + brightness;
        // Apply contrast
        val = factor * (val - 128.0f) + 128.0f;

        dst_data[i] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Effects::GammaCorrection(const Image& src, Image& dst, float gamma) {
    if (!src.isValid() || gamma <= 0.0f) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    // Precompute gamma table for speed
    uint8_t gamma_table[256];
    float inv_gamma = 1.0f / gamma;
    for (int i = 0; i < 256; ++i) {
        gamma_table[i] = static_cast<uint8_t>(std::clamp(std::pow(i / 255.0f, inv_gamma) * 255.0f, 0.0f, 255.0f));
    }

    for (size_t i = 0; i < src_data.size(); ++i) {
        if (ch == 4 && (i % 4 == 3)) {
            dst_data[i] = src_data[i];
            continue;
        }
        dst_data[i] = gamma_table[src_data[i]];
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Effects::Sepia(const Image& src, Image& dst) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    if (ch < 3) {
        Logger::getInstance().error("Sepia effect requires an RGB/RGBA image");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    for (size_t i = 0; i < src_data.size(); i += ch) {
        uint8_t r = src_data[i + 0];
        uint8_t g = src_data[i + 1];
        uint8_t b = src_data[i + 2];

        float sr = (r * 0.393f) + (g * 0.769f) + (b * 0.189f);
        float sg = (r * 0.349f) + (g * 0.686f) + (b * 0.168f);
        float sb = (r * 0.272f) + (g * 0.534f) + (b * 0.131f);

        dst_data[i + 0] = static_cast<uint8_t>(std::clamp(sr, 0.0f, 255.0f));
        dst_data[i + 1] = static_cast<uint8_t>(std::clamp(sg, 0.0f, 255.0f));
        dst_data[i + 2] = static_cast<uint8_t>(std::clamp(sb, 0.0f, 255.0f));
        
        if (ch == 4) dst_data[i + 3] = src_data[i + 3];
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Effects::ApplyLUT(const Image& src, Image& dst, const Image& lut) {
    if (!src.isValid() || !lut.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    // We expect a standard 3D Hald LUT flattened to 2D (512x512 for size 64, or 64x64x64)
    // For simplicity, we support a simple 3D color cube of size 16 (256x16 image)
    uint32_t lut_w = lut.getWidth();
    uint32_t lut_h = lut.getHeight();

    if (lut_w != 256 || lut_h != 16) {
        Logger::getInstance().error("Unsupported LUT dimensions. Only 256x16 3D color LUTs are supported.");
        return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    if (ch < 3) return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();
    const auto& lut_data = lut.getData();
    uint32_t lut_ch = lut.getChannels();

    for (size_t i = 0; i < src_data.size(); i += ch) {
        uint8_t r = src_data[i + 0];
        uint8_t g = src_data[i + 1];
        uint8_t b = src_data[i + 2];

        // Map RGB to LUT 2D coordinates
        // b maps to different blocks along X
        int b_block = b / 16; // 0 to 15 blocks
        int lut_x = b_block * 16 + (r / 16);
        int lut_y = g / 16;

        size_t lut_idx = (static_cast<size_t>(lut_y) * 256 + lut_x) * lut_ch;
        dst_data[i + 0] = lut_data[lut_idx + 0];
        dst_data[i + 1] = lut_data[lut_idx + 1];
        dst_data[i + 2] = lut_data[lut_idx + 2];

        if (ch == 4) dst_data[i + 3] = src_data[i + 3];
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Effects::Vignette(const Image& src, Image& dst, float intensity, float falloff) {
    if (!src.isValid() || intensity < 0.0f || falloff < 0.0f) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    float cx = w / 2.0f;
    float cy = h / 2.0f;
    float max_dist = std::sqrt(cx * cx + cy * cy);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy) / max_dist;

            // Apply falloff equation
            float factor = 1.0f - intensity * std::pow(dist, falloff);
            factor = std::clamp(factor, 0.0f, 1.0f);

            size_t idx = (static_cast<size_t>(y) * w + x) * ch;
            for (uint32_t c = 0; c < ch; ++c) {
                if (ch == 4 && c == 3) {
                    dst_data[idx + c] = src_data[idx + c];
                } else {
                    dst_data[idx + c] = static_cast<uint8_t>(std::clamp(src_data[idx + c] * factor, 0.0f, 255.0f));
                }
            }
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Effects::SimulateColorBlindness(const Image& src, Image& dst, ColorBlindType type) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    if (ch < 3) return PixelForgeErrorCode::ERR_UNSUPPORTED_FORMAT;

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    // Simulation conversion matrices in linear RGB space
    float mat[3][3] = {0};
    switch (type) {
        case ColorBlindType::PROTANOPIA:
            mat[0][0] = 0.56667f; mat[0][1] = 0.43333f; mat[0][2] = 0.00000f;
            mat[1][0] = 0.55833f; mat[1][1] = 0.44167f; mat[1][2] = 0.00000f;
            mat[2][0] = 0.00000f; mat[2][1] = 0.24167f; mat[2][2] = 0.75833f;
            break;
        case ColorBlindType::DEUTERANOPIA:
            mat[0][0] = 0.62500f; mat[0][1] = 0.37500f; mat[0][2] = 0.00000f;
            mat[1][0] = 0.70000f; mat[1][1] = 0.30000f; mat[1][2] = 0.00000f;
            mat[2][0] = 0.00000f; mat[2][1] = 0.30000f; mat[2][2] = 0.70000f;
            break;
        case ColorBlindType::TRITANOPIA:
            mat[0][0] = 0.95000f; mat[0][1] = 0.05000f; mat[0][2] = 0.00000f;
            mat[1][0] = 0.00000f; mat[1][1] = 0.43333f; mat[1][2] = 0.56667f;
            mat[2][0] = 0.00000f; mat[2][1] = 0.47500f; mat[2][2] = 0.52500f;
            break;
    }

    for (size_t i = 0; i < src_data.size(); i += ch) {
        float r = src_data[i + 0];
        float g = src_data[i + 1];
        float b = src_data[i + 2];

        float nr = r * mat[0][0] + g * mat[0][1] + b * mat[0][2];
        float ng = r * mat[1][0] + g * mat[1][1] + b * mat[1][2];
        float nb = r * mat[2][0] + g * mat[2][1] + b * mat[2][2];

        dst_data[i + 0] = static_cast<uint8_t>(std::clamp(nr, 0.0f, 255.0f));
        dst_data[i + 1] = static_cast<uint8_t>(std::clamp(ng, 0.0f, 255.0f));
        dst_data[i + 2] = static_cast<uint8_t>(std::clamp(nb, 0.0f, 255.0f));

        if (ch == 4) dst_data[i + 3] = src_data[i + 3];
    }
    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
