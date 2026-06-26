#ifndef PIXELFORGE_ANALYSIS_H
#define PIXELFORGE_ANALYSIS_H

#include "image.h"
#include "errors.h"
#include <vector>
#include <cstdint>

namespace PixelForge {

struct ImageStats {
    float mean_r = 0.0f;
    float mean_g = 0.0f;
    float mean_b = 0.0f;
    float variance_r = 0.0f;
    float variance_g = 0.0f;
    float variance_b = 0.0f;
    float entropy = 0.0f;
};

struct HoughLine {
    float rho = 0.0f;
    float theta = 0.0f;
    uint32_t votes = 0;
};

class Analysis {
public:
    // Basic image statistical properties
    static PixelForgeErrorCode ComputeStats(const Image& src, ImageStats& stats);

    // Otsu's thresholding method (automatically computes optimal threshold for binarization)
    static uint8_t ComputeOtsuThreshold(const Image& src);

    // Binarize image based on threshold
    static PixelForgeErrorCode Threshold(const Image& src, Image& dst, uint8_t threshold);

    // Connected Component Labeling (CCL) using two-pass algorithm.
    // Labels 8-connected regions and returns the number of components.
    static PixelForgeErrorCode LabelComponents(const Image& src, Image& dst, uint32_t& num_components);

    // Canny Edge Detector
    static PixelForgeErrorCode CannyEdges(const Image& src, Image& dst, float low_threshold, float high_threshold);

    // Hough Transform Line Detection (runs on binary/edge image)
    static PixelForgeErrorCode HoughLines(const Image& src, std::vector<HoughLine>& lines, uint32_t threshold_votes);

    // Compute Structural Similarity Index (SSIM) between two images
    static PixelForgeErrorCode ComputeSSIM(const Image& img1, const Image& img2, float& ssim_val);
};

} // namespace PixelForge

#endif // PIXELFORGE_ANALYSIS_H
