#ifndef PIXELFORGE_TRANSFORM_H
#define PIXELFORGE_TRANSFORM_H

#include "image.h"
#include "errors.h"

namespace PixelForge {

class Transform {
public:
    // Flip operations
    static PixelForgeErrorCode FlipHorizontal(const Image& src, Image& dst);
    static PixelForgeErrorCode FlipVertical(const Image& src, Image& dst);

    // Rotate operations (fast multiples of 90)
    static PixelForgeErrorCode Rotate90(const Image& src, Image& dst);
    static PixelForgeErrorCode Rotate180(const Image& src, Image& dst);
    static PixelForgeErrorCode Rotate270(const Image& src, Image& dst);

    // Arbitrary rotation using bilinear interpolation (angle in degrees)
    static PixelForgeErrorCode Rotate(const Image& src, Image& dst, float angle);

    // Translation (shift x and y, padding with background color)
    static PixelForgeErrorCode Translate(const Image& src, Image& dst, int dx, int dy);
};

} // namespace PixelForge

#endif // PIXELFORGE_TRANSFORM_H
