#include "convolution.h"
#include "logger.h"
#include <cmath>
#include <algorithm>

namespace PixelForge {

Kernel::Kernel(int w, int h, const std::vector<float>& wts, float s, float o)
    : width(w), height(h), weights(wts), scale(s), offset(o) {
    if (weights.size() < static_cast<size_t>(width * height)) {
        weights.resize(width * height, 0.0f);
    }
}

Kernel Convolution::GetGaussianBlur5x5() {
    std::vector<float> w = {
        2,  4,  5,  4, 2,
        4,  9, 12,  9, 4,
        5, 12, 15, 12, 5,
        4,  9, 12,  9, 4,
        2,  4,  5,  4, 2
    };
    return Kernel(5, 5, w, 159.0f, 0.0f);
}

Kernel Convolution::GetBoxBlur3x3() {
    std::vector<float> w = {
        1, 1, 1,
        1, 1, 1,
        1, 1, 1
    };
    return Kernel(3, 3, w, 9.0f, 0.0f);
}

Kernel Convolution::GetSharpen3x3() {
    std::vector<float> w = {
         0, -1,  0,
        -1,  5, -1,
         0, -1,  0
    };
    return Kernel(3, 3, w, 1.0f, 0.0f);
}

Kernel Convolution::GetSobelX3x3() {
    std::vector<float> w = {
        -1, 0, 1,
        -2, 0, 2,
        -1, 0, 1
    };
    return Kernel(3, 3, w, 1.0f, 128.0f); // offset by 128 to show negative values
}

Kernel Convolution::GetSobelY3x3() {
    std::vector<float> w = {
        -1, -2, -1,
         0,  0,  0,
         1,  2,  1
    };
    return Kernel(3, 3, w, 1.0f, 128.0f);
}

Kernel Convolution::GetLaplacian3x3() {
    std::vector<float> w = {
        0,  1, 0,
        1, -4, 1,
        0,  1, 0
    };
    return Kernel(3, 3, w, 1.0f, 128.0f);
}

Kernel Convolution::GetEmboss3x3() {
    std::vector<float> w = {
        -2, -1, 0,
        -1,  1, 1,
         0,  1, 2
    };
    return Kernel(3, 3, w, 1.0f, 128.0f);
}

PixelForgeErrorCode Convolution::Apply(const Image& src, Image& dst, const Kernel& kernel) {
    if (!src.isValid()) {
        Logger::getInstance().error("Convolution input image is invalid");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    if (kernel.width <= 0 || kernel.height <= 0 || (kernel.width % 2 == 0) || (kernel.height % 2 == 0)) {
        Logger::getInstance().error("Invalid kernel size. Kernels must have odd positive dimensions.");
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) {
        return err;
    }

    int kw = kernel.width;
    int kh = kernel.height;
    int kx_offset = kw / 2;
    int ky_offset = kh / 2;

    const std::vector<uint8_t>& src_data = src.getData();
    std::vector<uint8_t>& dst_data = dst.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            for (uint32_t c = 0; c < ch; ++c) {
                // Alpha channel usually doesn't get convolved
                if (ch == 4 && c == 3) {
                    size_t idx = (static_cast<size_t>(y) * w + x) * 4 + 3;
                    dst_data[idx] = src_data[idx];
                    continue;
                }

                float sum = 0.0f;
                for (int ky = 0; ky < kh; ++ky) {
                    int py = static_cast<int>(y) + ky - ky_offset;
                    // Clamp to boundary pixels
                    py = std::clamp(py, 0, static_cast<int>(h) - 1);

                    for (int kx = 0; kx < kw; ++kx) {
                        int px = static_cast<int>(x) + kx - kx_offset;
                        px = std::clamp(px, 0, static_cast<int>(w) - 1);

                        size_t src_idx = (static_cast<size_t>(py) * w + px) * ch + c;
                        float pixel_val = src_data[src_idx];
                        float weight = kernel.weights[ky * kw + kx];

                        sum += pixel_val * weight;
                    }
                }

                float result = (sum / kernel.scale) + kernel.offset;
                size_t dst_idx = (static_cast<size_t>(y) * w + x) * ch + c;
                dst_data[dst_idx] = static_cast<uint8_t>(std::clamp(result, 0.0f, 255.0f));
            }
        }
    }

    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
