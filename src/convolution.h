#ifndef PIXELFORGE_CONVOLUTION_H
#define PIXELFORGE_CONVOLUTION_H

#include "image.h"
#include "errors.h"
#include <vector>
#include <string>

namespace PixelForge {

struct Kernel {
    int width = 0;
    int height = 0;
    std::vector<float> weights;
    float scale = 1.0f;
    float offset = 0.0f;

    Kernel() = default;
    Kernel(int w, int h, const std::vector<float>& wts, float s = 1.0f, float o = 0.0f);
};

class Convolution {
public:
    static Kernel GetGaussianBlur5x5();
    static Kernel GetBoxBlur3x3();
    static Kernel GetSharpen3x3();
    static Kernel GetSobelX3x3();
    static Kernel GetSobelY3x3();
    static Kernel GetLaplacian3x3();
    static Kernel GetEmboss3x3();

    // Apply a custom kernel convolution to an image
    static PixelForgeErrorCode Apply(const Image& src, Image& dst, const Kernel& kernel);
};

} // namespace PixelForge

#endif // PIXELFORGE_CONVOLUTION_H
