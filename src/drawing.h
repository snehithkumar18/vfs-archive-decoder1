#ifndef PIXELFORGE_DRAWING_H
#define PIXELFORGE_DRAWING_H

#include "image.h"
#include "errors.h"
#include <string>

namespace PixelForge {

struct ColorRGBA {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    ColorRGBA() = default;
    ColorRGBA(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
};

struct Point2D {
    int x = 0;
    int y = 0;
    Point2D() = default;
    Point2D(int x_, int y_) : x(x_), y(y_) {}
};

class Drawing {
public:
    // Draw a single pixel with alpha blending
    static void DrawPixel(Image& img, int x, int y, const ColorRGBA& color);

    // Draw helper shapes
    static PixelForgeErrorCode DrawLine(Image& img, int x0, int y0, int x1, int y1, const ColorRGBA& color);
    static PixelForgeErrorCode DrawRect(Image& img, int x, int y, int width, int height, const ColorRGBA& color, bool fill = false);
    static PixelForgeErrorCode DrawCircle(Image& img, int cx, int cy, int radius, const ColorRGBA& color, bool fill = false);
    static PixelForgeErrorCode DrawEllipse(Image& img, int cx, int cy, int rx, int ry, const ColorRGBA& color, bool fill = false);
    
    // Draw arbitrary polygons (scanline filled if requested)
    static PixelForgeErrorCode DrawPolygon(Image& img, const std::vector<Point2D>& points, const ColorRGBA& color, bool fill = false);

    // Draw Bezier curves
    static PixelForgeErrorCode DrawBezierQuadratic(Image& img, const Point2D& p0, const Point2D& p1, const Point2D& p2, const ColorRGBA& color);
    static PixelForgeErrorCode DrawBezierCubic(Image& img, const Point2D& p0, const Point2D& p1, const Point2D& p2, const Point2D& p3, const ColorRGBA& color);

    // Draw text using a built-in bitmap font (8x8 monochrome font)
    static PixelForgeErrorCode DrawText(Image& img, int x, int y, const std::string& text, const ColorRGBA& color);
};

} // namespace PixelForge

#endif // PIXELFORGE_DRAWING_H
