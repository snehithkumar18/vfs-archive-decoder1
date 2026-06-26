#include "enhancement.h"
#include "logger.h"
#include "histogram.h"
#include "convolution.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

namespace PixelForge {

PixelForgeErrorCode Enhancement::MedianFilter(const Image& src, Image& dst, int window_size) {
    if (!src.isValid() || window_size <= 0 || (window_size % 2 == 0)) {
        Logger::getInstance().error("Invalid median filter window size: must be odd and positive");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    int offset = window_size / 2;
    std::vector<uint8_t> window(static_cast<size_t>(window_size) * window_size);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            for (uint32_t c = 0; c < ch; ++c) {
                // Keep alpha channel unchanged
                if (ch == 4 && c == 3) {
                    size_t idx = (static_cast<size_t>(y) * w + x) * 4 + 3;
                    dst_data[idx] = src_data[idx];
                    continue;
                }

                size_t count = 0;
                for (int wy = -offset; wy <= offset; ++wy) {
                    int py = std::clamp(static_cast<int>(y) + wy, 0, static_cast<int>(h) - 1);
                    for (int wx = -offset; wx <= offset; ++wx) {
                        int px = std::clamp(static_cast<int>(x) + wx, 0, static_cast<int>(w) - 1);
                        window[count++] = src_data[(static_cast<size_t>(py) * w + px) * ch + c];
                    }
                }

                std::sort(window.begin(), window.begin() + count);
                size_t median_idx = count / 2;
                dst_data[(static_cast<size_t>(y) * w + x) * ch + c] = window[median_idx];
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Enhancement::BilateralFilter(const Image& src, Image& dst, float sigma_s, float sigma_r) {
    if (!src.isValid() || sigma_s <= 0.0f || sigma_r <= 0.0f) {
        Logger::getInstance().error("Invalid bilateral filter parameters");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    int radius = static_cast<int>(std::ceil(2.0f * sigma_s));
    float den_s = 2.0f * sigma_s * sigma_s;
    float den_r = 2.0f * sigma_r * sigma_r;

    for (int y = 0; y < static_cast<int>(h); ++y) {
        for (int x = 0; x < static_cast<int>(w); ++x) {
            for (uint32_t c = 0; c < ch; ++c) {
                if (ch == 4 && c == 3) {
                    size_t idx = (static_cast<size_t>(y) * w + x) * 4 + 3;
                    dst_data[idx] = src_data[idx];
                    continue;
                }

                float sum_weights = 0.0f;
                float sum_values = 0.0f;
                float center_val = src_data[(static_cast<size_t>(y) * w + x) * ch + c] / 255.0f;

                for (int wy = -radius; wy <= radius; ++wy) {
                    int py = std::clamp(y + wy, 0, static_cast<int>(h) - 1);
                    for (int wx = -radius; wx <= radius; ++wx) {
                        int px = std::clamp(x + wx, 0, static_cast<int>(w) - 1);

                        float val = src_data[(static_cast<size_t>(py) * w + px) * ch + c] / 255.0f;

                        // Spatial difference
                        float d_s2 = static_cast<float>(wx * wx + wy * wy);
                        // Range/intensity difference
                        float d_r2 = (val - center_val) * (val - center_val);

                        float weight = std::exp(-d_s2 / den_s - d_r2 / den_r);

                        sum_weights += weight;
                        sum_values += val * weight;
                    }
                }

                float final_val = (sum_values / sum_weights) * 255.0f;
                dst_data[(static_cast<size_t>(y) * w + x) * ch + c] = static_cast<uint8_t>(std::clamp(final_val, 0.0f, 255.0f));
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Enhancement::UnsharpMask(const Image& src, Image& dst, float amount, float threshold) {
    if (!src.isValid() || amount < 0.0f) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    Image blurred;
    PixelForgeErrorCode err = Convolution::Apply(src, blurred, Convolution::GetGaussianBlur5x5());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    const auto& blurred_data = blurred.getData();
    auto& dst_data = dst.getData();

    for (size_t i = 0; i < src_data.size(); ++i) {
        if (ch == 4 && (i % 4 == 3)) {
            dst_data[i] = src_data[i];
            continue;
        }

        float orig = src_data[i];
        float blur = blurred_data[i];
        float diff = orig - blur;

        if (std::abs(diff) >= threshold) {
            float final_val = orig + amount * diff;
            dst_data[i] = static_cast<uint8_t>(std::clamp(final_val, 0.0f, 255.0f));
        } else {
            dst_data[i] = src_data[i];
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Enhancement::MatchHistogram(const Image& src, const Image& ref, Image& dst) {
    if (!src.isValid() || !ref.isValid() || src.getChannels() != ref.getChannels()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    ImageHistogram src_hist, ref_hist;
    Histogram::Compute(src, src_hist);
    Histogram::Compute(ref, ref_hist);

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    auto match_channel = [](const std::vector<uint32_t>& s_h, const std::vector<uint32_t>& r_h, 
                            const std::vector<uint8_t>& s_d, std::vector<uint8_t>& d_d, 
                            uint32_t ch_offset, uint32_t step, size_t total_p, size_t ref_total_p) {
        // Compute CDFs
        std::vector<double> s_cdf(256, 0.0), r_cdf(256, 0.0);
        s_cdf[0] = static_cast<double>(s_h[0]) / total_p;
        r_cdf[0] = static_cast<double>(r_h[0]) / ref_total_p;
        for (int i = 1; i < 256; ++i) {
            s_cdf[i] = s_cdf[i - 1] + static_cast<double>(s_h[i]) / total_p;
            r_cdf[i] = r_cdf[i - 1] + static_cast<double>(r_h[i]) / ref_total_p;
        }

        // Map src intensity to ref intensity
        std::vector<uint8_t> lut(256, 0);
        for (int i = 0; i < 256; ++i) {
            double s_val = s_cdf[i];
            int mapped = 0;
            double min_diff = 1e9;
            for (int j = 0; j < 256; ++j) {
                double diff = std::abs(s_val - r_cdf[j]);
                if (diff < min_diff) {
                    min_diff = diff;
                    mapped = j;
                }
            }
            lut[i] = static_cast<uint8_t>(mapped);
        }

        // Apply mapping
        for (size_t i = ch_offset; i < s_d.size(); i += step) {
            d_d[i] = lut[s_d[i]];
        }
    };

    size_t src_pixels = w * h;
    size_t ref_pixels = ref.getWidth() * ref.getHeight();

    if (ch == 1) {
        match_channel(src_hist.gray, ref_hist.gray, src_data, dst_data, 0, 1, src_pixels, ref_pixels);
    } else {
        match_channel(src_hist.r, ref_hist.r, src_data, dst_data, 0, ch, src_pixels, ref_pixels);
        match_channel(src_hist.g, ref_hist.g, src_data, dst_data, 1, ch, src_pixels, ref_pixels);
        match_channel(src_hist.b, ref_hist.b, src_data, dst_data, 2, ch, src_pixels, ref_pixels);
        if (ch == 4) {
            for (size_t i = 3; i < src_data.size(); i += 4) dst_data[i] = src_data[i];
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Enhancement::AdaptiveHistogramEqualization(const Image& src, Image& dst, int block_size) {
    if (!src.isValid() || block_size <= 0) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    int half_block = block_size / 2;

    for (int y = 0; y < static_cast<int>(h); ++y) {
        for (int x = 0; x < static_cast<int>(w); ++x) {
            // Find bounding box for block
            int y_start = std::max(0, y - half_block);
            int y_end = std::min(static_cast<int>(h) - 1, y + half_block);
            int x_start = std::max(0, x - half_block);
            int x_end = std::min(static_cast<int>(w) - 1, x + half_block);

            int total_p = (y_end - y_start + 1) * (x_end - x_start + 1);

            for (uint32_t c = 0; c < ch; ++c) {
                if (ch == 4 && c == 3) {
                    size_t idx = (static_cast<size_t>(y) * w + x) * 4 + 3;
                    dst_data[idx] = src_data[idx];
                    continue;
                }

                uint8_t val = src_data[(static_cast<size_t>(y) * w + x) * ch + c];

                // Count pixels in the neighborhood that are smaller than or equal to current pixel
                int rank = 0;
                for (int py = y_start; py <= y_end; ++py) {
                    for (int px = x_start; px <= x_end; ++px) {
                        uint8_t neighbor_val = src_data[(static_cast<size_t>(py) * w + px) * ch + c];
                        if (neighbor_val <= val) {
                            rank++;
                        }
                    }
                }

                // Map rank directly to 0-255 scale
                float mapped = (static_cast<float>(rank) / total_p) * 255.0f;
                dst_data[(static_cast<size_t>(y) * w + x) * ch + c] = static_cast<uint8_t>(std::clamp(mapped, 0.0f, 255.0f));
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
