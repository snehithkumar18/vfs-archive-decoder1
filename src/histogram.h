#ifndef PIXELFORGE_HISTOGRAM_H
#define PIXELFORGE_HISTOGRAM_H

#include "image.h"
#include "errors.h"
#include <vector>
#include <array>

namespace PixelForge {

struct ImageHistogram {
    std::vector<uint32_t> r;
    std::vector<uint32_t> g;
    std::vector<uint32_t> b;
    std::vector<uint32_t> gray;
};

class Histogram {
public:
    // Compute histograms for each channel
    static PixelForgeErrorCode Compute(const Image& src, ImageHistogram& hist);

    // Apply histogram equalization to enhance contrast
    static PixelForgeErrorCode Equalize(const Image& src, Image& dst);

    // Apply contrast stretching (normalize pixel range to [min_val, max_val])
    static PixelForgeErrorCode Stretch(const Image& src, Image& dst, uint8_t min_val = 0, uint8_t max_val = 255);
};

} // namespace PixelForge

#endif // PIXELFORGE_HISTOGRAM_H
