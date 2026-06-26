#ifndef PIXELFORGE_ENHANCEMENT_H
#define PIXELFORGE_ENHANCEMENT_H

#include "image.h"
#include "errors.h"

namespace PixelForge {

class Enhancement {
public:
    // Denoise using Median Filter (non-linear filter, great for salt-and-pepper noise)
    static PixelForgeErrorCode MedianFilter(const Image& src, Image& dst, int window_size = 3);

    // Denoise using Bilateral Filter (edge-preserving smoothing)
    static PixelForgeErrorCode BilateralFilter(const Image& src, Image& dst, float sigma_s = 3.0f, float sigma_r = 0.1f);

    // Contrast Enhancement via Unsharp Masking (sharpening by adding high-frequency detail)
    static PixelForgeErrorCode UnsharpMask(const Image& src, Image& dst, float amount = 1.0f, float threshold = 0.0f);

    // Histogram matching: forces the histogram of src to resemble reference ref
    static PixelForgeErrorCode MatchHistogram(const Image& src, const Image& ref, Image& dst);

    // Local Contrast Enhancement (Adaptive Histogram Equalization - AHE)
    static PixelForgeErrorCode AdaptiveHistogramEqualization(const Image& src, Image& dst, int block_size = 16);
};

} // namespace PixelForge

#endif // PIXELFORGE_ENHANCEMENT_H
