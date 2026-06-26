#include "drawing.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace PixelForge {

// A simple 8x8 bitmap font representation for ASCII characters 32 to 127.
// Each byte represents a row of 8 pixels (1 bit per pixel).
static const uint8_t g_font8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 32 (space)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // 33 (!)
    {0x36, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00}, // 34 (")
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // 35 (#)
    {0x1C, 0x3E, 0x61, 0x3C, 0x06, 0x3E, 0x1C, 0x08}, // 36 ($)
    {0x63, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x63, 0x00}, // 37 (%)
    {0x1C, 0x36, 0x1C, 0x3A, 0x6E, 0x36, 0x6E, 0x00}, // 38 (&)
    {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00}, // 39 (')
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, // 40 (()
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, // 41 ())
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // 42 (*)
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, // 43 (+)
    {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00}, // 44 (,)
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // 45 (-)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // 46 (.)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00}, // 47 (/)
    {0x3E, 0x63, 0x67, 0x6F, 0x7B, 0x63, 0x3E, 0x00}, // 48 (0)
    {0x0C, 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // 49 (1)
    {0x3E, 0x63, 0x06, 0x1C, 0x30, 0x61, 0x7F, 0x00}, // 50 (2)
    {0x3E, 0x63, 0x06, 0x1C, 0x06, 0x63, 0x3E, 0x00}, // 51 (3)
    {0x06, 0x0E, 0x1E, 0x36, 0x7F, 0x06, 0x06, 0x00}, // 52 (4)
    {0x7F, 0x60, 0x7E, 0x03, 0x03, 0x63, 0x3E, 0x00}, // 53 (5)
    {0x1C, 0x30, 0x60, 0x7E, 0x63, 0x63, 0x3E, 0x00}, // 54 (6)
    {0x7F, 0x63, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x00}, // 55 (7)
    {0x3E, 0x63, 0x63, 0x3E, 0x63, 0x63, 0x3E, 0x00}, // 56 (8)
    {0x3E, 0x63, 0x63, 0x7F, 0x03, 0x06, 0x3C, 0x00}, // 57 (9)
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // 58 (:)
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // 59 (;)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // 60 (<)
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, // 61 (=)
    {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00}, // 62 (>)
    {0x3E, 0x63, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00}, // 63 (?)
    {0x3E, 0x63, 0x6F, 0x6B, 0x6F, 0x60, 0x3E, 0x00}, // 64 (@)
    {0x18, 0x3C, 0x66, 0x66, 0x7F, 0x66, 0x66, 0x00}, // 65 (A)
    {0x7E, 0x33, 0x33, 0x3E, 0x33, 0x33, 0x7E, 0x00}, // 66 (B)
    {0x1E, 0x33, 0x60, 0x60, 0x60, 0x33, 0x1E, 0x00}, // 67 (C)
    {0x7C, 0x36, 0x33, 0x33, 0x33, 0x36, 0x7C, 0x00}, // 68 (D)
    {0x7F, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7F, 0x00}, // 69 (E)
    {0x7F, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00}, // 70 (F)
    {0x3E, 0x63, 0x60, 0x6F, 0x63, 0x63, 0x3E, 0x00}, // 71 (G)
    {0x66, 0x66, 0x66, 0x7F, 0x66, 0x66, 0x66, 0x00}, // 72 (H)
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // 73 (I)
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00}, // 74 (J)
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, // 75 (K)
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7F, 0x00}, // 76 (L)
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // 77 (M)
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, // 78 (N)
    {0x3E, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00}, // 79 (O)
    {0x7E, 0x63, 0x63, 0x7E, 0x60, 0x60, 0x60, 0x00}, // 80 (P)
    {0x3E, 0x63, 0x63, 0x63, 0x6B, 0x66, 0x3D, 0x00}, // 81 (Q)
    {0x7E, 0x63, 0x63, 0x7E, 0x78, 0x6C, 0x66, 0x00}, // 82 (R)
    {0x3E, 0x63, 0x60, 0x3E, 0x03, 0x63, 0x3E, 0x00}, // 83 (S)
    {0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 84 (T)
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00}, // 85 (U)
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // 86 (V)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // 87 (W)
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, // 88 (X)
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x00}, // 89 (Y)
    {0x7F, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7F, 0x00}, // 90 (Z)
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, // 91 ([)
    {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x00}, // 92 (\)
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, // 93 (])
    {0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00}, // 94 (^)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00}, // 95 (_)
    {0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // 96 (`)
    {0x00, 0x00, 0x3E, 0x03, 0x3E, 0x63, 0x3F, 0x00}, // 97 (a)
    {0x60, 0x60, 0x7E, 0x63, 0x63, 0x63, 0x7E, 0x00}, // 98 (b)
    {0x00, 0x00, 0x3E, 0x60, 0x60, 0x63, 0x3E, 0x00}, // 99 (c)
    {0x03, 0x03, 0x3F, 0x63, 0x63, 0x63, 0x3F, 0x00}, // 100 (d)
    {0x00, 0x00, 0x3E, 0x63, 0x7F, 0x60, 0x3E, 0x00}, // 101 (e)
    {0x1C, 0x36, 0x30, 0x78, 0x30, 0x30, 0x78, 0x00}, // 102 (f)
    {0x00, 0x00, 0x3F, 0x63, 0x63, 0x3F, 0x03, 0x3E}, // 103 (g)
    {0x60, 0x60, 0x7E, 0x63, 0x63, 0x63, 0x63, 0x00}, // 104 (h)
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 105 (i)
    {0x06, 0x00, 0x0E, 0x06, 0x06, 0x06, 0x06, 0x3C}, // 106 (j)
    {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00}, // 107 (k)
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 108 (l)
    {0x00, 0x00, 0x6B, 0x7F, 0x6B, 0x6B, 0x6B, 0x00}, // 109 (m)
    {0x00, 0x00, 0x7E, 0x63, 0x63, 0x63, 0x63, 0x00}, // 110 (n)
    {0x00, 0x00, 0x3E, 0x63, 0x63, 0x63, 0x3E, 0x00}, // 111 (o)
    {0x00, 0x00, 0x7E, 0x63, 0x63, 0x7E, 0x60, 0x60}, // 112 (p)
    {0x00, 0x00, 0x3F, 0x63, 0x63, 0x3F, 0x03, 0x03}, // 113 (q)
    {0x00, 0x00, 0x7E, 0x60, 0x60, 0x60, 0x60, 0x00}, // 114 (r)
    {0x00, 0x00, 0x3E, 0x60, 0x3E, 0x03, 0x3E, 0x00}, // 115 (s)
    {0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00}, // 116 (t)
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x63, 0x3F, 0x00}, // 117 (u)
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x3C, 0x18, 0x00}, // 118 (v)
    {0x00, 0x00, 0x63, 0x6B, 0x6B, 0x7F, 0x36, 0x00}, // 119 (w)
    {0x00, 0x00, 0x63, 0x3C, 0x18, 0x3C, 0x63, 0x00}, // 120 (x)
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x3F, 0x03, 0x3E}, // 121 (y)
    {0x00, 0x00, 0x7F, 0x0C, 0x18, 0x30, 0x7F, 0x00}, // 122 (z)
    {0x0C, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0C, 0x00}, // 123 ({)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // 124 (|)
    {0x30, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x30, 0x00}, // 125 (})
    {0x00, 0x00, 0x00, 0x76, 0xDC, 0x00, 0x00, 0x00}, // 126 (~)
    {0x10, 0x28, 0x54, 0xAA, 0x54, 0x28, 0x10, 0x00}  // 127 (delete block)
};

void Drawing::DrawPixel(Image& img, int x, int y, const ColorRGBA& color) {
    if (x < 0 || x >= static_cast<int>(img.getWidth()) || y < 0 || y >= static_cast<int>(img.getHeight())) {
        return;
    }

    uint32_t ch = img.getChannels();
    uint8_t* ptr = img.getPixelPointer(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    if (!ptr) return;

    if (color.a == 255 || ch < 3) {
        // Direct copy for opaque colors
        ptr[0] = color.r;
        if (ch >= 3) {
            ptr[1] = color.g;
            ptr[2] = color.b;
        }
        if (ch == 4) {
            ptr[3] = color.a;
        }
    } else {
        // Alpha blending
        float alpha = color.a / 255.0f;
        float inv_alpha = 1.0f - alpha;

        ptr[0] = static_cast<uint8_t>(color.r * alpha + ptr[0] * inv_alpha);
        ptr[1] = static_cast<uint8_t>(color.g * alpha + ptr[1] * inv_alpha);
        ptr[2] = static_cast<uint8_t>(color.b * alpha + ptr[2] * inv_alpha);
        if (ch == 4) {
            ptr[3] = static_cast<uint8_t>(std::max(static_cast<int>(ptr[3]), static_cast<int>(color.a)));
        }
    }
}

PixelForgeErrorCode Drawing::DrawLine(Image& img, int x0, int y0, int x1, int y1, const ColorRGBA& color) {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    // Bresenham's line algorithm
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        DrawPixel(img, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Drawing::DrawRect(Image& img, int x, int y, int width, int height, const ColorRGBA& color, bool fill) {
    if (!img.isValid() || width <= 0 || height <= 0) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    if (fill) {
        for (int py = y; py < y + height; ++py) {
            for (int px = x; px < x + width; ++px) {
                DrawPixel(img, px, py, color);
            }
        }
    } else {
        DrawLine(img, x, y, x + width - 1, y, color);
        DrawLine(img, x, y + height - 1, x + width - 1, y + height - 1, color);
        DrawLine(img, x, y, x, y + height - 1, color);
        DrawLine(img, x + width - 1, y, x + width - 1, y + height - 1, color);
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Drawing::DrawCircle(Image& img, int cx, int cy, int radius, const ColorRGBA& color, bool fill) {
    if (!img.isValid() || radius < 0) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    // Bresenham's circle algorithm
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    auto draw_circle_helper = [&](int cx, int cy, int x, int y) {
        if (fill) {
            // Draw horizontal spans to fill the circle
            DrawLine(img, cx - x, cy + y, cx + x, cy + y, color);
            DrawLine(img, cx - x, cy - y, cx + x, cy - y, color);
            DrawLine(img, cx - y, cy + x, cx + y, cy + x, color);
            DrawLine(img, cx - y, cy - x, cx + y, cy - x, color);
        } else {
            DrawPixel(img, cx + x, cy + y, color);
            DrawPixel(img, cx - x, cy + y, color);
            DrawPixel(img, cx + x, cy - y, color);
            DrawPixel(img, cx - x, cy - y, color);
            DrawPixel(img, cx + y, cy + x, color);
            DrawPixel(img, cx - y, cy + x, color);
            DrawPixel(img, cx + y, cy - x, color);
            DrawPixel(img, cx - y, cy - x, color);
        }
    };

    draw_circle_helper(cx, cy, x, y);
    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        draw_circle_helper(cx, cy, x, y);
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Drawing::DrawText(Image& img, int x, int y, const std::string& text, const ColorRGBA& color) {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    int cur_x = x;
    int cur_y = y;

    for (char c : text) {
        if (c == '\n') {
            cur_x = x;
            cur_y += 8;
            continue;
        }
        if (c == '\r') {
            continue;
        }

        uint8_t font_idx = static_cast<uint8_t>(c);
        if (font_idx < 32 || font_idx > 127) {
            font_idx = 63; // '?'
        }
        font_idx -= 32;

        for (int row = 0; row < 8; ++row) {
            uint8_t byte = g_font8x8[font_idx][row];
            for (int col = 0; col < 8; ++col) {
                // Pixel is set if LSB is at the right of the bitmask
                if ((byte & (1 << (7 - col))) != 0) {
                    DrawPixel(img, cur_x + col, cur_y + row, color);
                }
            }
        }
        cur_x += 8;
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Drawing::DrawEllipse(Image& img, int cx, int cy, int rx, int ry, const ColorRGBA& color, bool fill) {
    if (!img.isValid() || rx < 0 || ry < 0) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    if (fill) {
        for (int y = -ry; y <= ry; ++y) {
            int x_bound = static_cast<int>(std::round(rx * std::sqrt(1.0 - (y * y) / static_cast<double>(ry * ry))));
            DrawLine(img, cx - x_bound, cy + y, cx + x_bound, cy + y, color);
        }
    } else {
        // Bresenham's ellipse outline algorithm
        int x = 0;
        int y = ry;
        double d1 = (ry * ry) - (rx * rx * ry) + (0.25 * rx * rx);
        double dx = 2.0 * ry * ry * x;
        double dy = 2.0 * rx * rx * y;

        while (dx < dy) {
            DrawPixel(img, cx + x, cy + y, color);
            DrawPixel(img, cx - x, cy + y, color);
            DrawPixel(img, cx + x, cy - y, color);
            DrawPixel(img, cx - x, cy - y, color);

            if (d1 < 0) {
                x++;
                dx = dx + (2.0 * ry * ry);
                d1 = d1 + dx + (ry * ry);
            } else {
                x++;
                y--;
                dx = dx + (2.0 * ry * ry);
                dy = dy - (2.0 * rx * rx);
                d1 = d1 + dx - dy + (ry * ry);
            }
        }

        double d2 = ((ry * ry) * ((x + 0.5) * (x + 0.5))) + ((rx * rx) * ((y - 1) * (y - 1))) - (rx * rx * ry * ry);
        while (y >= 0) {
            DrawPixel(img, cx + x, cy + y, color);
            DrawPixel(img, cx - x, cy + y, color);
            DrawPixel(img, cx + x, cy - y, color);
            DrawPixel(img, cx - x, cy - y, color);

            if (d2 > 0) {
                y--;
                dy = dy - (2.0 * rx * rx);
                d2 = d2 + (rx * rx) - dy;
            } else {
                y--;
                x++;
                dx = dx + (2.0 * ry * ry);
                dy = dy - (2.0 * rx * rx);
                d2 = d2 + dx - dy + (rx * rx);
            }
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Drawing::DrawPolygon(Image& img, const std::vector<Point2D>& points, const ColorRGBA& color, bool fill) {
    if (!img.isValid() || points.size() < 3) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    if (!fill) {
        for (size_t i = 0; i < points.size(); ++i) {
            const auto& p1 = points[i];
            const auto& p2 = points[(i + 1) % points.size()];
            DrawLine(img, p1.x, p1.y, p2.x, p2.y, color);
        }
    } else {
        // Scanline polygon filling
        int min_y = points[0].y;
        int max_y = points[0].y;
        for (const auto& p : points) {
            min_y = std::min(min_y, p.y);
            max_y = std::max(max_y, p.y);
        }

        min_y = std::max(0, min_y);
        max_y = std::min(static_cast<int>(img.getHeight()) - 1, max_y);

        for (int y = min_y; y <= max_y; ++y) {
            std::vector<int> node_x;
            for (size_t i = 0; i < points.size(); ++i) {
                const auto& p1 = points[i];
                const auto& p2 = points[(i + 1) % points.size()];

                if ((p1.y < y && p2.y >= y) || (p2.y < y && p1.y >= y)) {
                    // Compute intersection x coordinate
                    int x = static_cast<int>(p1.x + (y - p1.y) * static_cast<double>(p2.x - p1.x) / (p2.y - p1.y));
                    node_x.push_back(x);
                }
            }

            std::sort(node_x.begin(), node_x.end());

            for (size_t i = 0; i < node_x.size(); i += 2) {
                if (i + 1 >= node_x.size()) break;
                int x_start = std::max(0, node_x[i]);
                int x_end = std::min(static_cast<int>(img.getWidth()) - 1, node_x[i + 1]);
                for (int x = x_start; x <= x_end; ++x) {
                    DrawPixel(img, x, y, color);
                }
            }
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Drawing::DrawBezierQuadratic(Image& img, const Point2D& p0, const Point2D& p1, const Point2D& p2, const ColorRGBA& color) {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    // Approximate step count based on control point distance
    double d1 = std::sqrt((p1.x - p0.x)*(p1.x - p0.x) + (p1.y - p0.y)*(p1.y - p0.y));
    double d2 = std::sqrt((p2.x - p1.x)*(p2.x - p1.x) + (p2.y - p1.y)*(p2.y - p1.y));
    int steps = static_cast<int>(std::clamp((d1 + d2) / 2.0, 10.0, 100.0));

    int prev_x = p0.x;
    int prev_y = p0.y;

    for (int i = 1; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        double one_minus_t = 1.0 - t;

        double w0 = one_minus_t * one_minus_t;
        double w1 = 2.0 * t * one_minus_t;
        double w2 = t * t;

        int cur_x = static_cast<int>(std::round(w0 * p0.x + w1 * p1.x + w2 * p2.x));
        int cur_y = static_cast<int>(std::round(w0 * p0.y + w1 * p1.y + w2 * p2.y));

        DrawLine(img, prev_x, prev_y, cur_x, cur_y, color);
        prev_x = cur_x;
        prev_y = cur_y;
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Drawing::DrawBezierCubic(Image& img, const Point2D& p0, const Point2D& p1, const Point2D& p2, const Point2D& p3, const ColorRGBA& color) {
    if (!img.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    double d1 = std::sqrt((p1.x - p0.x)*(p1.x - p0.x) + (p1.y - p0.y)*(p1.y - p0.y));
    double d2 = std::sqrt((p2.x - p1.x)*(p2.x - p1.x) + (p2.y - p1.y)*(p2.y - p1.y));
    double d3 = std::sqrt((p3.x - p2.x)*(p3.x - p2.x) + (p3.y - p2.y)*(p3.y - p2.y));
    int steps = static_cast<int>(std::clamp((d1 + d2 + d3) / 3.0, 10.0, 150.0));

    int prev_x = p0.x;
    int prev_y = p0.y;

    for (int i = 1; i <= steps; ++i) {
        double t = static_cast<double>(i) / steps;
        double one_minus_t = 1.0 - t;

        double w0 = one_minus_t * one_minus_t * one_minus_t;
        double w1 = 3.0 * t * one_minus_t * one_minus_t;
        double w2 = 3.0 * t * t * one_minus_t;
        double w3 = t * t * t;

        int cur_x = static_cast<int>(std::round(w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x));
        int cur_y = static_cast<int>(std::round(w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y));

        DrawLine(img, prev_x, prev_y, cur_x, cur_y, color);
        prev_x = cur_x;
        prev_y = cur_y;
    }
    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
