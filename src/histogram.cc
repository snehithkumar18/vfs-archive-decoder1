#include "histogram.h"
#include "logger.h"
#include <algorithm>

namespace PixelForge {

PixelForgeErrorCode Histogram::Compute(const Image& src, ImageHistogram& hist) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    hist.r.assign(256, 0);
    hist.g.assign(256, 0);
    hist.b.assign(256, 0);
    hist.gray.assign(256, 0);

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();
    const auto& data = src.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * ch;
            if (ch >= 3) {
                uint8_t r = data[idx + 0];
                uint8_t g = data[idx + 1];
                uint8_t b = data[idx + 2];
                hist.r[r]++;
                hist.g[g]++;
                hist.b[b]++;
                
                uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
                hist.gray[gray]++;
            } else if (ch == 1) {
                uint8_t gray = data[idx];
                hist.gray[gray]++;
                hist.r[gray]++;
                hist.g[gray]++;
                hist.b[gray]++;
            }
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Histogram::Equalize(const Image& src, Image& dst) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    ImageHistogram hist;
    Compute(src, hist);

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    uint32_t total_pixels = w * h;

    if (ch == 1) {
        // Grayscale equalization
        std::vector<uint32_t> cdf(256, 0);
        cdf[0] = hist.gray[0];
        for (int i = 1; i < 256; ++i) cdf[i] = cdf[i - 1] + hist.gray[i];

        uint32_t cdf_min = 0;
        for (int i = 0; i < 256; ++i) {
            if (cdf[i] > 0) {
                cdf_min = cdf[i];
                break;
            }
        }

        std::array<uint8_t, 256> lut;
        for (int i = 0; i < 256; ++i) {
            if (total_pixels - cdf_min == 0) {
                lut[i] = static_cast<uint8_t>(i);
            } else {
                lut[i] = static_cast<uint8_t>(std::round(255.0f * (cdf[i] - cdf_min) / (total_pixels - cdf_min)));
            }
        }

        for (size_t i = 0; i < src_data.size(); ++i) {
            dst_data[i] = lut[src_data[i]];
        }
    } else {
        // Equalize channels independently or on Y channel of YCbCr (better)
        // For simplicity, we equalize R, G, B independently
        std::vector<uint32_t> cdf_r(256, 0), cdf_g(256, 0), cdf_b(256, 0);
        cdf_r[0] = hist.r[0]; cdf_g[0] = hist.g[0]; cdf_b[0] = hist.b[0];

        for (int i = 1; i < 256; ++i) {
            cdf_r[i] = cdf_r[i - 1] + hist.r[i];
            cdf_g[i] = cdf_g[i - 1] + hist.g[i];
            cdf_b[i] = cdf_b[i - 1] + hist.b[i];
        }

        auto find_min = [](const std::vector<uint32_t>& cdf) {
            for (int i = 0; i < 256; ++i) if (cdf[i] > 0) return cdf[i];
            return 0u;
        };

        uint32_t min_r = find_min(cdf_r), min_g = find_min(cdf_g), min_b = find_min(cdf_b);

        std::array<uint8_t, 256> lut_r, lut_g, lut_b;
        for (int i = 0; i < 256; ++i) {
            lut_r[i] = (total_pixels - min_r == 0) ? i : static_cast<uint8_t>(std::round(255.0f * (cdf_r[i] - min_r) / (total_pixels - min_r)));
            lut_g[i] = (total_pixels - min_g == 0) ? i : static_cast<uint8_t>(std::round(255.0f * (cdf_g[i] - min_g) / (total_pixels - min_g)));
            lut_b[i] = (total_pixels - min_b == 0) ? i : static_cast<uint8_t>(std::round(255.0f * (cdf_b[i] - min_b) / (total_pixels - min_b)));
        }

        for (size_t i = 0; i < src_data.size(); i += ch) {
            dst_data[i + 0] = lut_r[src_data[i + 0]];
            dst_data[i + 1] = lut_g[src_data[i + 1]];
            dst_data[i + 2] = lut_b[src_data[i + 2]];
            if (ch == 4) dst_data[i + 3] = src_data[i + 3];
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Histogram::Stretch(const Image& src, Image& dst, uint8_t min_val, uint8_t max_val) {
    if (!src.isValid() || min_val >= max_val) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    uint8_t actual_min = 255, actual_max = 0;
    for (size_t i = 0; i < src_data.size(); ++i) {
        if (ch == 4 && (i % 4 == 3)) continue; // Skip alpha
        actual_min = std::min(actual_min, src_data[i]);
        actual_max = std::max(actual_max, src_data[i]);
    }

    if (actual_max == actual_min) {
        std::memcpy(dst_data.data(), src_data.data(), src_data.size());
        return PixelForgeErrorCode::SUCCESS;
    }

    float range_diff = static_cast<float>(actual_max - actual_min);
    float target_diff = static_cast<float>(max_val - min_val);

    for (size_t i = 0; i < src_data.size(); ++i) {
        if (ch == 4 && (i % 4 == 3)) {
            dst_data[i] = src_data[i];
            continue;
        }
        float stretched = (static_cast<float>(src_data[i] - actual_min) / range_diff) * target_diff + min_val;
        dst_data[i] = static_cast<uint8_t>(std::clamp(stretched, 0.0f, 255.0f));
    }

    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
