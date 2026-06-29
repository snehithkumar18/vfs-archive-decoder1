#include "image_processor_extra.h"
#include <cmath>
#include <algorithm>
#include <map>

namespace PixelForge {

// Helper to clamp values
static inline float clamp(float val, float min_val, float max_val) {
    return std::max(min_val, std::min(val, max_val));
}

// ---------------------------------------------------------
// Antialiased Drawing Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::draw_line_aa(Image& img, Point2D p1, Point2D p2, Pixel color) {
    int w = img.get_width();
    int h = img.get_height();

    auto plot = [&](int x, int y, float c) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            Pixel bg = img.get_pixel(x, y);
            Pixel blended;
            blended.r = static_cast<uint8_t>(bg.r * (1.0f - c) + color.r * c);
            blended.g = static_cast<uint8_t>(bg.g * (1.0f - c) + color.g * c);
            blended.b = static_cast<uint8_t>(bg.b * (1.0f - c) + color.b * c);
            blended.a = 255;
            img.set_pixel(x, y, blended);
        }
    };

    bool steep = std::abs(p2.y - p1.y) > std::abs(p2.x - p1.x);
    if (steep) {
        std::swap(p1.x, p1.y);
        std::swap(p2.x, p2.y);
    }
    if (p1.x > p2.x) {
        std::swap(p1.x, p2.x);
        std::swap(p1.y, p2.y);
    }

    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    // Handle first endpoint
    int xpxl1 = static_cast<int>(p1.x);
    float yend = p1.y + gradient * (xpxl1 - p1.x);
    float xgap = 1.0f - (p1.x + 0.5f - static_cast<int>(p1.x + 0.5f));
    int ypxl1 = static_cast<int>(yend);
    if (steep) {
        plot(ypxl1, xpxl1, (1.0f - (yend - std::floor(yend))) * xgap);
        plot(ypxl1 + 1, xpxl1, (yend - std::floor(yend)) * xgap);
    } else {
        plot(xpxl1, ypxl1, (1.0f - (yend - std::floor(yend))) * xgap);
        plot(xpxl1, ypxl1 + 1, (yend - std::floor(yend)) * xgap);
    }
    float intery = yend + gradient;

    // Handle second endpoint
    int xpxl2 = static_cast<int>(p2.x);
    yend = p2.y + gradient * (xpxl2 - p2.x);
    xgap = p2.x + 0.5f - static_cast<int>(p2.x + 0.5f);
    int ypxl2 = static_cast<int>(yend);
    if (steep) {
        plot(ypxl2, xpxl2, (1.0f - (yend - std::floor(yend))) * xgap);
        plot(ypxl2 + 1, xpxl2, (yend - std::floor(yend)) * xgap);
    } else {
        plot(xpxl2, ypxl2, (1.0f - (yend - std::floor(yend))) * xgap);
        plot(xpxl2, ypxl2 + 1, (yend - std::floor(yend)) * xgap);
    }

    // Main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            plot(static_cast<int>(intery), x, 1.0f - (intery - std::floor(intery)));
            plot(static_cast<int>(intery) + 1, x, intery - std::floor(intery));
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; ++x) {
            plot(x, static_cast<int>(intery), 1.0f - (intery - std::floor(intery)));
            plot(x, static_cast<int>(intery) + 1, intery - std::floor(intery));
            intery += gradient;
        }
    }
}

void ImageProcessorExtra::draw_circle_aa(Image& img, Point2D center, float radius, Pixel color) {
    int w = img.get_width();
    int h = img.get_height();

    int min_x = std::max(0, static_cast<int>(center.x - radius - 2));
    int max_x = std::min(w - 1, static_cast<int>(center.x + radius + 2));
    int min_y = std::max(0, static_cast<int>(center.y - radius - 2));
    int max_y = std::min(h - 1, static_cast<int>(center.y + radius + 2));

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            float dx = x - center.x;
            float dy = y - center.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            float diff = std::abs(dist - radius);

            if (diff < 1.0f) {
                float coverage = 1.0f - diff;
                Pixel bg = img.get_pixel(x, y);
                Pixel blended;
                blended.r = static_cast<uint8_t>(bg.r * (1.0f - coverage) + color.r * coverage);
                blended.g = static_cast<uint8_t>(bg.g * (1.0f - coverage) + color.g * coverage);
                blended.b = static_cast<uint8_t>(bg.b * (1.0f - coverage) + color.b * coverage);
                blended.a = 255;
                img.set_pixel(x, y, blended);
            }
        }
    }
}

void ImageProcessorExtra::draw_bezier_quadratic(Image& img, Point2D p0, Point2D p1, Point2D p2, Pixel color) {
    int steps = 100;
    Point2D prev = p0;
    for (int i = 1; i <= steps; ++i) {
        float t = i / static_cast<float>(steps);
        float u = 1.0f - t;
        Point2D curr;
        curr.x = u * u * p0.x + 2.0f * u * t * p1.x + t * t * p2.x;
        curr.y = u * u * p0.y + 2.0f * u * t * p1.y + t * t * p2.y;
        draw_line_aa(img, prev, curr, color);
        prev = curr;
    }
}

void ImageProcessorExtra::draw_bezier_cubic(Image& img, Point2D p0, Point2D p1, Point2D p2, Point2D p3, Pixel color) {
    int steps = 150;
    Point2D prev = p0;
    for (int i = 1; i <= steps; ++i) {
        float t = i / static_cast<float>(steps);
        float u = 1.0f - t;
        Point2D curr;
        curr.x = u * u * u * p0.x + 3.0f * u * u * t * p1.x + 3.0f * u * t * t * p2.x + t * t * t * p3.x;
        curr.y = u * u * u * p0.y + 3.0f * u * u * t * p1.y + 3.0f * u * t * t * p2.y + t * t * t * p3.y;
        draw_line_aa(img, prev, curr, color);
        prev = curr;
    }
}

void ImageProcessorExtra::fill_polygon(Image& img, const std::vector<Point2D>& vertices, Pixel color) {
    if (vertices.size() < 3) return;
    int h = img.get_height();
    int w = img.get_width();

    // Find bounding box
    float min_y_f = vertices[0].y;
    float max_y_f = vertices[0].y;
    for (const auto& v : vertices) {
        min_y_f = std::min(min_y_f, v.y);
        max_y_f = std::max(max_y_f, v.y);
    }

    int min_y = std::max(0, static_cast<int>(std::floor(min_y_f)));
    int max_y = std::min(h - 1, static_cast<int>(std::ceil(max_y_f)));

    for (int y = min_y; y <= max_y; ++y) {
        std::vector<float> node_x;
        size_t j = vertices.size() - 1;
        float y_coord = static_cast<float>(y) + 0.5f;

        for (size_t i = 0; i < vertices.size(); ++i) {
            if ((vertices[i].y < y_coord && vertices[j].y >= y_coord) ||
                (vertices[j].y < y_coord && vertices[i].y >= y_coord)) {
                float x = vertices[i].x + (y_coord - vertices[i].y) / (vertices[j].y - vertices[i].y) * (vertices[j].x - vertices[i].x);
                node_x.push_back(x);
            }
            j = i;
        }

        std::sort(node_x.begin(), node_x.end());

        for (size_t i = 0; i < node_x.size(); i += 2) {
            if (i + 1 >= node_x.size()) break;
            int start_x = std::max(0, static_cast<int>(std::ceil(node_x[i])));
            int end_x = std::min(w - 1, static_cast<int>(std::floor(node_x[i + 1])));
            for (int x = start_x; x <= end_x; ++x) {
                img.set_pixel(x, y, color);
            }
        }
    }
}

// ---------------------------------------------------------
// Color Space Conversions
// ---------------------------------------------------------

ColorHSL ImageProcessorExtra::rgb_to_hsl(Pixel p) {
    float r = p.r / 255.0f;
    float g = p.g / 255.0f;
    float b = p.b / 255.0f;

    float max_val = std::max({r, g, b});
    float min_val = std::min({r, g, b});
    float h = 0.0f, s = 0.0f, l = (max_val + min_val) / 2.0f;

    if (max_val != min_val) {
        float d = max_val - min_val;
        s = l > 0.5f ? d / (2.0f - max_val - min_val) : d / (max_val + min_val);
        if (max_val == r) {
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        } else if (max_val == g) {
            h = (b - r) / d + 2.0f;
        } else if (max_val == b) {
            h = (r - g) / d + 4.0f;
        }
        h /= 6.0f;
    }

    return {h * 360.0f, s, l};
}

Pixel ImageProcessorExtra::hsl_to_rgb(ColorHSL hsl) {
    float h = hsl.h / 360.0f;
    float s = hsl.s;
    float l = hsl.l;

    auto hue_to_rgb = [](float p, float q, float t) {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f / 2.0f) return q;
        if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
    };

    float r, g, b;
    if (s == 0.0f) {
        r = g = b = l; // achromatic
    } else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = hue_to_rgb(p, q, h + 1.0f / 3.0f);
        g = hue_to_rgb(p, q, h);
        b = hue_to_rgb(p, q, h - 1.0f / 3.0f);
    }

    Pixel p;
    p.r = static_cast<uint8_t>(clamp(r * 255.0f, 0.0f, 255.0f));
    p.g = static_cast<uint8_t>(clamp(g * 255.0f, 0.0f, 255.0f));
    p.b = static_cast<uint8_t>(clamp(b * 255.0f, 0.0f, 255.0f));
    p.a = 255;
    return p;
}

ColorHSV ImageProcessorExtra::rgb_to_hsv(Pixel p) {
    float r = p.r / 255.0f;
    float g = p.g / 255.0f;
    float b = p.b / 255.0f;

    float max_val = std::max({r, g, b});
    float min_val = std::min({r, g, b});
    float h = 0.0f, s = 0.0f, v = max_val;

    float d = max_val - min_val;
    s = max_val == 0.0f ? 0.0f : d / max_val;

    if (max_val != min_val) {
        if (max_val == r) {
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        } else if (max_val == g) {
            h = (b - r) / d + 2.0f;
        } else if (max_val == b) {
            h = (r - g) / d + 4.0f;
        }
        h /= 6.0f;
    }

    return {h * 360.0f, s, v};
}

Pixel ImageProcessorExtra::hsv_to_rgb(ColorHSV hsv) {
    float h = hsv.h / 60.0f;
    float s = hsv.s;
    float v = hsv.v;

    int i = static_cast<int>(std::floor(h)) % 6;
    float f = h - std::floor(h);
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    float r = 0, g = 0, b = 0;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }

    Pixel pixel;
    pixel.r = static_cast<uint8_t>(clamp(r * 255.0f, 0.0f, 255.0f));
    pixel.g = static_cast<uint8_t>(clamp(g * 255.0f, 0.0f, 255.0f));
    pixel.b = static_cast<uint8_t>(clamp(b * 255.0f, 0.0f, 255.0f));
    pixel.a = 255;
    return pixel;
}

ColorYUV ImageProcessorExtra::rgb_to_yuv(Pixel p) {
    float r = p.r;
    float g = p.g;
    float b = p.b;

    float y = 0.299f * r + 0.587f * g + 0.114f * b;
    float u = -0.1687f * r - 0.3313f * g + 0.5f * b + 128.0f;
    float v = 0.5f * r - 0.4187f * g - 0.0813f * b + 128.0f;

    return {y, u, v};
}

Pixel ImageProcessorExtra::yuv_to_rgb(ColorYUV yuv) {
    float y = yuv.y;
    float u = yuv.u - 128.0f;
    float v = yuv.v - 128.0f;

    float r = y + 1.402f * v;
    float g = y - 0.34414f * u - 0.71414f * v;
    float b = y + 1.772f * u;

    Pixel p;
    p.r = static_cast<uint8_t>(clamp(r, 0.0f, 255.0f));
    p.g = static_cast<uint8_t>(clamp(g, 0.0f, 255.0f));
    p.b = static_cast<uint8_t>(clamp(b, 0.0f, 255.0f));
    p.a = 255;
    return p;
}

ColorCMYK ImageProcessorExtra::rgb_to_cmyk(Pixel p) {
    float r = p.r / 255.0f;
    float g = p.g / 255.0f;
    float b = p.b / 255.0f;

    float k = 1.0f - std::max({r, g, b});
    float c = (k == 1.0f) ? 0.0f : (1.0f - r - k) / (1.0f - k);
    float m = (k == 1.0f) ? 0.0f : (1.0f - g - k) / (1.0f - k);
    float y = (k == 1.0f) ? 0.0f : (1.0f - b - k) / (1.0f - k);

    return {c, m, y, k};
}

Pixel ImageProcessorExtra::cmyk_to_rgb(ColorCMYK cmyk) {
    float c = cmyk.c;
    float m = cmyk.m;
    float y = cmyk.y;
    float k = cmyk.k;

    float r = (1.0f - c) * (1.0f - k);
    float g = (1.0f - m) * (1.0f - k);
    float b = (1.0f - y) * (1.0f - k);

    Pixel p;
    p.r = static_cast<uint8_t>(clamp(r * 255.0f, 0.0f, 255.0f));
    p.g = static_cast<uint8_t>(clamp(g * 255.0f, 0.0f, 255.0f));
    p.b = static_cast<uint8_t>(clamp(b * 255.0f, 0.0f, 255.0f));
    p.a = 255;
    return p;
}

ColorLAB ImageProcessorExtra::rgb_to_lab(Pixel p) {
    // RGB to XYZ
    float r = p.r / 255.0f;
    float g = p.g / 255.0f;
    float b = p.b / 255.0f;

    r = (r > 0.04045f) ? std::pow((r + 0.055f) / 1.055f, 2.4f) : r / 12.92f;
    g = (g > 0.04045f) ? std::pow((g + 0.055f) / 1.055f, 2.4f) : g / 12.92f;
    b = (b > 0.04045f) ? std::pow((b + 0.055f) / 1.055f, 2.4f) : b / 12.92f;

    float x = r * 0.4124564f + g * 0.3575761f + b * 0.1804375f;
    float y = r * 0.2126729f + g * 0.7151522f + b * 0.0721750f;
    float z = r * 0.0193339f + g * 0.1191920f + b * 0.9503041f;

    // XYZ to LAB
    x /= 0.95047f;
    y /= 1.00000f;
    z /= 1.08883f;

    auto f = [](float t) {
        return (t > 0.008856f) ? std::pow(t, 1.0f / 3.0f) : (7.787f * t) + (16.0f / 116.0f);
    };

    float fx = f(x);
    float fy = f(y);
    float fz = f(z);

    float l = (116.0f * fy) - 16.0f;
    float a = 500.0f * (fx - fy);
    float _b = 200.0f * (fy - fz);

    return {l, a, _b};
}

Pixel ImageProcessorExtra::lab_to_rgb(ColorLAB lab) {
    float l = lab.l;
    float a = lab.a;
    float b = lab.b;

    float fy = (l + 16.0f) / 116.0f;
    float fx = fy + (a / 500.0f);
    float fz = fy - (b / 200.0f);

    auto f_inv = [](float t) {
        float t3 = t * t * t;
        return (t3 > 0.008856f) ? t3 : (t - 16.0f / 116.0f) / 7.787f;
    };

    float x = 0.95047f * f_inv(fx);
    float y = 1.00000f * f_inv(fy);
    float z = 1.08883f * f_inv(fz);

    // XYZ to RGB
    float r = x * 3.2404542f + y * -1.5371385f + z * -0.4985314f;
    float g = x * -0.9692660f + y * 1.8760108f + z * 0.0415560f;
    float _b = x * 0.0556434f + y * -0.2040259f + z * 1.0572252f;

    auto gamma = [](float t) {
        return (t > 0.0031308f) ? (1.055f * std::pow(t, 1.0f / 2.4f)) - 0.055f : 12.92f * t;
    };

    r = gamma(r);
    g = gamma(g);
    _b = gamma(_b);

    Pixel p;
    p.r = static_cast<uint8_t>(clamp(r * 255.0f, 0.0f, 255.0f));
    p.g = static_cast<uint8_t>(clamp(g * 255.0f, 0.0f, 255.0f));
    p.b = static_cast<uint8_t>(clamp(_b * 255.0f, 0.0f, 255.0f));
    p.a = 255;
    return p;
}

// ---------------------------------------------------------
// Advanced Filters
// ---------------------------------------------------------

void ImageProcessorExtra::apply_vignette(Image& img, float radius, float softness) {
    int w = img.get_width();
    int h = img.get_height();
    float cx = w / 2.0f;
    float cy = h / 2.0f;
    float max_dist = std::sqrt(cx * cx + cy * cy) * radius;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);

            float factor = 1.0f;
            if (dist > max_dist * (1.0f - softness)) {
                float t = (dist - max_dist * (1.0f - softness)) / (max_dist * softness);
                factor = 1.0f - clamp(t, 0.0f, 1.0f);
            }

            Pixel p = img.get_pixel(x, y);
            p.r = static_cast<uint8_t>(p.r * factor);
            p.g = static_cast<uint8_t>(p.g * factor);
            p.b = static_cast<uint8_t>(p.b * factor);
            img.set_pixel(x, y, p);
        }
    }
}

void ImageProcessorExtra::apply_bilateral_filter(Image& img, int radius, float sigma_d, float sigma_r) {
    int w = img.get_width();
    int h = img.get_height();
    Image temp = img;

    float two_sigma_d_sq = 2.0f * sigma_d * sigma_d;
    float two_sigma_r_sq = 2.0f * sigma_r * sigma_r;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
            float w_sum = 0.0f;
            Pixel center_p = temp.get_pixel(x, y);

            for (int ky = -radius; ky <= radius; ++ky) {
                int ny = clamp(y + ky, 0, h - 1);
                for (int kx = -radius; kx <= radius; ++kx) {
                    int nx = clamp(x + kx, 0, w - 1);

                    Pixel neighbor_p = temp.get_pixel(nx, ny);

                    float spatial_dist_sq = kx * kx + ky * ky;
                    float color_dist_sq = std::pow(center_p.r - neighbor_p.r, 2) +
                                          std::pow(center_p.g - neighbor_p.g, 2) +
                                          std::pow(center_p.b - neighbor_p.b, 2);

                    float w_spatial = std::exp(-spatial_dist_sq / two_sigma_d_sq);
                    float w_color = std::exp(-color_dist_sq / two_sigma_r_sq);
                    float weight = w_spatial * w_color;

                    sum_r += neighbor_p.r * weight;
                    sum_g += neighbor_p.g * weight;
                    sum_b += neighbor_p.b * weight;
                    w_sum += weight;
                }
            }

            Pixel out;
            out.r = static_cast<uint8_t>(clamp(sum_r / w_sum, 0.0f, 255.0f));
            out.g = static_cast<uint8_t>(clamp(sum_g / w_sum, 0.0f, 255.0f));
            out.b = static_cast<uint8_t>(clamp(sum_b / w_sum, 0.0f, 255.0f));
            out.a = 255;
            img.set_pixel(x, y, out);
        }
    }
}

void ImageProcessorExtra::apply_adaptive_histogram_equalization(Image& img, int grid_size, float clip_limit) {
    int w = img.get_width();
    int h = img.get_height();

    // Convert image to HSL to equalize Luminance channel only
    std::vector<ColorHSL> hsl_pixels(w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            hsl_pixels[y * w + x] = rgb_to_hsl(img.get_pixel(x, y));
        }
    }

    int tiles_x = grid_size;
    int tiles_y = grid_size;
    int tile_w = w / tiles_x;
    int tile_h = h / tiles_y;

    if (tile_w == 0 || tile_h == 0) return;

    std::vector<std::vector<int>> histograms(tiles_x * tiles_y, std::vector<int>(256, 0));

    // Compute local histograms
    for (int ty = 0; ty < tiles_y; ++ty) {
        for (int tx = 0; tx < tiles_x; ++tx) {
            int hist_idx = ty * tiles_x + tx;
            int start_x = tx * tile_w;
            int start_y = ty * tile_h;

            for (int y = start_y; y < start_y + tile_h; ++y) {
                for (int x = start_x; x < start_x + tile_w; ++x) {
                    int val = static_cast<int>(hsl_pixels[y * w + x].l * 255.0f);
                    histograms[hist_idx][val]++;
                }
            }

            // Clip histogram
            if (clip_limit > 0.0f) {
                int limit = static_cast<int>(clip_limit * (tile_w * tile_h) / 256.0f);
                int excess = 0;
                for (int i = 0; i < 256; ++i) {
                    if (histograms[hist_idx][i] > limit) {
                        excess += histograms[hist_idx][i] - limit;
                        histograms[hist_idx][i] = limit;
                    }
                }
                int redist = excess / 256;
                for (int i = 0; i < 256; ++i) {
                    histograms[hist_idx][i] += redist;
                }
            }
        }
    }

    // Compute CDFs
    std::vector<std::vector<float>> cdfs(tiles_x * tiles_y, std::vector<float>(256, 0.0f));
    for (int i = 0; i < tiles_x * tiles_y; ++i) {
        float sum = 0.0f;
        int total = tile_w * tile_h;
        for (int j = 0; j < 256; ++j) {
            sum += histograms[i][j];
            cdfs[i][j] = sum / total;
        }
    }

    // Interpolate CDFs for each pixel
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float tx = (x - tile_w / 2.0f) / tile_w;
            float ty = (y - tile_h / 2.0f) / tile_h;

            int tx0 = static_cast<int>(std::floor(tx));
            int tx1 = tx0 + 1;
            int ty0 = static_cast<int>(std::floor(ty));
            int ty1 = ty0 + 1;

            tx0 = clamp(tx0, 0, tiles_x - 1);
            tx1 = clamp(tx1, 0, tiles_x - 1);
            ty0 = clamp(ty0, 0, tiles_y - 1);
            ty1 = clamp(ty1, 0, tiles_y - 1);

            float fx = tx - std::floor(tx);
            float fy = ty - std::floor(ty);

            int val = static_cast<int>(hsl_pixels[y * w + x].l * 255.0f);

            float c00 = cdfs[ty0 * tiles_x + tx0][val];
            float c01 = cdfs[ty0 * tiles_x + tx1][val];
            float c10 = cdfs[ty1 * tiles_x + tx0][val];
            float c11 = cdfs[ty1 * tiles_x + tx1][val];

            float c = (1.0f - fx) * (1.0f - fy) * c00 +
                      fx * (1.0f - fy) * c01 +
                      (1.0f - fx) * fy * c10 +
                      fx * fy * c11;

            hsl_pixels[y * w + x].l = clamp(c, 0.0f, 1.0f);
            img.set_pixel(x, y, hsl_to_rgb(hsl_pixels[y * w + x]));
        }
    }
}

// ---------------------------------------------------------
// Antialiased Drawing & Text Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::draw_text_aa(Image& img, int x, int y, const std::string& text, int scale, Pixel color) {
    struct Stroke {
        int8_t x1, y1, x2, y2;
    };
    
    auto get_strokes = [](char c) -> std::vector<Stroke> {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        switch (c) {
            case 'A': return {{2,8,4,0}, {4,0,6,8}, {3,5,5,5}};
            case 'B': return {{2,0,2,8}, {2,0,6,0}, {6,0,6,4}, {6,4,2,4}, {6,4,6,8}, {6,8,2,8}};
            case 'C': return {{6,1,3,1}, {3,1,3,7}, {3,7,6,7}};
            case 'D': return {{2,0,2,8}, {2,0,5,0}, {5,0,5,8}, {5,8,2,8}};
            case 'E': return {{2,0,2,8}, {2,0,6,0}, {2,4,5,4}, {2,8,6,8}};
            case 'F': return {{2,0,2,8}, {2,0,6,0}, {2,4,5,4}};
            case 'G': return {{6,2,6,1}, {6,1,3,1}, {3,1,3,7}, {3,7,6,7}, {6,7,6,4}, {6,4,4,4}};
            case 'H': return {{2,0,2,8}, {6,0,6,8}, {2,4,6,4}};
            case 'I': return {{2,0,6,0}, {4,0,4,8}, {2,8,6,8}};
            case 'J': return {{5,0,5,6}, {5,6,3,8}, {3,8,2,7}};
            case 'K': return {{2,0,2,8}, {2,4,6,0}, {2,4,6,8}};
            case 'L': return {{2,0,2,8}, {2,8,6,8}};
            case 'M': return {{2,8,2,0}, {2,0,4,4}, {4,4,6,0}, {6,0,6,8}};
            case 'N': return {{2,8,2,0}, {2,0,6,8}, {6,8,6,0}};
            case 'O': return {{3,0,5,0}, {5,0,6,1}, {6,1,6,7}, {6,7,5,8}, {5,8,3,8}, {3,8,2,7}, {2,7,2,1}, {2,1,3,0}};
            case 'P': return {{2,8,2,0}, {2,0,6,0}, {6,0,6,4}, {6,4,2,4}};
            case 'Q': return {{3,0,5,0}, {5,0,6,1}, {6,1,6,7}, {6,7,5,8}, {5,8,3,8}, {3,8,2,7}, {2,7,2,1}, {2,1,3,0}, {5,6,7,8}};
            case 'R': return {{2,8,2,0}, {2,0,6,0}, {6,0,6,4}, {6,4,2,4}, {4,4,6,8}};
            case 'S': return {{6,1,3,1}, {3,1,3,4}, {3,4,6,4}, {6,4,6,7}, {6,7,3,7}};
            case 'T': return {{2,0,6,0}, {4,0,4,8}};
            case 'U': return {{2,0,2,7}, {2,7,3,8}, {3,8,5,8}, {5,8,6,7}, {6,7,6,0}};
            case 'V': return {{2,0,4,8}, {4,8,6,0}};
            case 'W': return {{2,0,2,8}, {2,8,4,5}, {4,5,6,8}, {6,8,6,0}};
            case 'X': return {{2,0,6,8}, {6,0,2,8}};
            case 'Y': return {{2,0,4,4}, {6,0,4,4}, {4,4,4,8}};
            case 'Z': return {{2,0,6,0}, {6,0,2,8}, {2,8,6,8}};
            case '0': return {{2,0,6,0}, {6,0,6,8}, {6,8,2,8}, {2,8,2,0}, {6,0,2,8}};
            case '1': return {{3,2,4,0}, {4,0,4,8}, {2,8,6,8}};
            case '2': return {{2,2,2,0}, {2,0,6,0}, {6,0,6,4}, {6,4,2,8}, {2,8,6,8}};
            case '3': return {{2,0,6,0}, {6,0,6,8}, {6,8,2,8}, {3,4,6,4}};
            case '4': return {{2,0,2,4}, {2,4,6,4}, {5,0,5,8}};
            case '5': return {{6,0,2,0}, {2,0,2,4}, {2,4,6,4}, {6,4,6,8}, {6,8,2,8}};
            case '6': return {{6,0,2,0}, {2,0,2,8}, {2,8,6,8}, {6,8,6,4}, {6,4,2,4}};
            case '7': return {{2,0,6,0}, {6,0,3,8}};
            case '8': return {{2,0,6,0}, {6,0,6,8}, {6,8,2,8}, {2,8,2,0}, {2,4,6,4}};
            case '9': return {{2,4,6,4}, {6,4,6,0}, {6,0,2,0}, {2,0,2,4}, {6,4,6,8}, {6,8,2,8}};
            case '-': return {{3,4,5,4}};
            case '.': return {{4,7,4,8}};
            case ',': return {{4,7,4,8}, {4,8,3,9}};
            case '!': return {{4,0,4,5}, {4,7,4,8}};
            case '?': return {{2,2,2,0}, {2,0,6,0}, {6,0,6,4}, {6,4,4,4}, {4,4,4,6}, {4,8,4,8}};
            case '+': return {{3,4,5,4}, {4,3,4,5}};
            case '=': return {{3,3,5,3}, {3,5,5,5}};
            case '/': return {{2,8,6,0}};
            case '\\': return {{2,0,6,8}};
            case '*': return {{3,3,5,5}, {5,3,3,5}, {4,2,4,6}, {2,4,6,4}};
            default: return {};
        }
    };

    int cursor_x = x;
    for (char c : text) {
        if (c == ' ') {
            cursor_x += 8 * scale;
            continue;
        }
        auto strokes = get_strokes(c);
        for (const auto& s : strokes) {
            Point2D p1{ static_cast<float>(cursor_x + s.x1 * scale), static_cast<float>(y + s.y1 * scale) };
            Point2D p2{ static_cast<float>(cursor_x + s.x2 * scale), static_cast<float>(y + s.y2 * scale) };
            draw_line_aa(img, p1, p2, color);
        }
        cursor_x += 10 * scale;
    }
}

// ---------------------------------------------------------
// 3D LUT Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_3d_lut(Image& img, const std::vector<Pixel>& lut_table, int lut_size) {
    if (lut_table.size() < static_cast<size_t>(lut_size * lut_size * lut_size)) {
        return;
    }

    int w = img.get_width();
    int h = img.get_height();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Pixel p = img.get_pixel(x, y);

            float r_val = (p.r / 255.0f) * (lut_size - 1);
            float g_val = (p.g / 255.0f) * (lut_size - 1);
            float b_val = (p.b / 255.0f) * (lut_size - 1);

            int r0 = static_cast<int>(std::floor(r_val));
            int r1 = std::min(r0 + 1, lut_size - 1);
            int g0 = static_cast<int>(std::floor(g_val));
            int g1 = std::min(g0 + 1, lut_size - 1);
            int b0 = static_cast<int>(std::floor(b_val));
            int b1 = std::min(b0 + 1, lut_size - 1);

            float dr = r_val - r0;
            float dg = g_val - g0;
            float db = b_val - b0;

            auto get_lut_pixel = [&](int r, int g, int b) -> Pixel {
                int index = r + g * lut_size + b * lut_size * lut_size;
                return lut_table[index];
            };

            Pixel c000 = get_lut_pixel(r0, g0, b0);
            Pixel c100 = get_lut_pixel(r1, g0, b0);
            Pixel c010 = get_lut_pixel(r0, g1, b0);
            Pixel c110 = get_lut_pixel(r1, g1, b0);
            Pixel c001 = get_lut_pixel(r0, g0, b1);
            Pixel c101 = get_lut_pixel(r1, g0, b1);
            Pixel c011 = get_lut_pixel(r0, g1, b1);
            Pixel c111 = get_lut_pixel(r1, g1, b1);

            float r_c00 = c000.r * (1.0f - dr) + c100.r * dr;
            float r_c10 = c010.r * (1.0f - dr) + c110.r * dr;
            float r_c01 = c001.r * (1.0f - dr) + c101.r * dr;
            float r_c11 = c011.r * (1.0f - dr) + c111.r * dr;
            float r_c0 = r_c00 * (1.0f - dg) + r_c10 * dg;
            float r_c1 = r_c01 * (1.0f - dg) + r_c11 * dg;
            float r_final = r_c0 * (1.0f - db) + r_c1 * db;

            float g_c00 = c000.g * (1.0f - dr) + c100.g * dr;
            float g_c10 = c010.g * (1.0f - dr) + c110.g * dr;
            float g_c01 = c001.g * (1.0f - dr) + c101.g * dr;
            float g_c11 = c011.g * (1.0f - dr) + c111.g * dr;
            float g_c0 = g_c00 * (1.0f - dg) + r_c10 * dg; // wait, let's use g_c10 instead of r_c10!
            g_c0 = g_c00 * (1.0f - dg) + g_c10 * dg;
            float g_c1 = g_c01 * (1.0f - dg) + g_c11 * dg;
            float g_final = g_c0 * (1.0f - db) + g_c1 * db;

            float b_c00 = c000.b * (1.0f - dr) + c100.b * dr;
            float b_c10 = c010.b * (1.0f - dr) + c110.b * dr;
            float b_c01 = c001.b * (1.0f - dr) + c101.b * dr;
            float b_c11 = c011.b * (1.0f - dr) + c111.b * dr;
            float b_c0 = b_c00 * (1.0f - dg) + b_c10 * dg;
            float b_c1 = b_c01 * (1.0f - dg) + b_c11 * dg;
            float b_final = b_c0 * (1.0f - db) + b_c1 * db;

            Pixel out;
            out.r = static_cast<uint8_t>(clamp(r_final, 0.0f, 255.0f));
            out.g = static_cast<uint8_t>(clamp(g_final, 0.0f, 255.0f));
            out.b = static_cast<uint8_t>(clamp(b_final, 0.0f, 255.0f));
            out.a = p.a;
            img.set_pixel(x, y, out);
        }
    }
}

// ---------------------------------------------------------
// 3D Perspective Projection Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_3d_perspective_projection(Image& img, float yaw, float pitch, float roll, float fov, float distance) {
    int w = img.get_width();
    int h = img.get_height();
    Image src = img;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            img.set_pixel(x, y, Pixel{0, 0, 0, 255});
        }
    }

    float rad_yaw = yaw * 3.14159265f / 180.0f;
    float rad_pitch = pitch * 3.14159265f / 180.0f;
    float rad_roll = roll * 3.14159265f / 180.0f;

    float cy = std::cos(rad_yaw), sy = std::sin(rad_yaw);
    float cp = std::cos(rad_pitch), sp = std::sin(rad_pitch);
    float cr = std::cos(rad_roll), sr = std::sin(rad_roll);

    float m00 = cy * cr - sy * sp * sr;
    float m01 = -cy * sr - sy * sp * cr;
    float m02 = -sy * cp;
    float m10 = cp * sr;
    float m11 = cp * cr;
    float m12 = -sp;
    float m20 = sy * cr + cy * sp * sr;
    float m21 = -sy * sr + cy * sp * cr;
    float m22 = cy * cp;

    float f = (w / 2.0f) / std::tan(fov * 3.14159265f / 360.0f);
    float cx = w / 2.0f;
    float cy_img = h / 2.0f;

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            float sx = dx - cx;
            float sy_screen = dy - cy_img;

            float denominator = m02 * sx + m12 * sy_screen + m22 * f;
            if (std::abs(denominator) < 1e-5f) continue;

            float t = (m22 * distance) / denominator;
            if (t < 0.0f) continue;

            float wx = t * sx;
            float wy = t * sy_screen;
            float wz = -distance + t * f;

            float px = m00 * wx + m10 * wy + m20 * wz;
            float py = m01 * wx + m11 * wy + m21 * wz;

            float src_x = px + cx;
            float src_y = py + cy_img;

            if (src_x >= 0.0f && src_x < w - 1 && src_y >= 0.0f && src_y < h - 1) {
                int x0 = static_cast<int>(std::floor(src_x));
                int x1 = x0 + 1;
                int y0 = static_cast<int>(std::floor(src_y));
                int y1 = y0 + 1;

                float tx = src_x - x0;
                float ty = src_y - y0;

                Pixel p00 = src.get_pixel(x0, y0);
                Pixel p10 = src.get_pixel(x1, y0);
                Pixel p01 = src.get_pixel(x0, y1);
                Pixel p11 = src.get_pixel(x1, y1);

                Pixel blended;
                blended.r = static_cast<uint8_t>(
                    (1.0f - tx) * (1.0f - ty) * p00.r +
                    tx * (1.0f - ty) * p10.r +
                    (1.0f - tx) * ty * p01.r +
                    tx * ty * p11.r
                );
                blended.g = static_cast<uint8_t>(
                    (1.0f - tx) * (1.0f - ty) * p00.g +
                    tx * (1.0f - ty) * p10.g +
                    (1.0f - tx) * ty * p01.g +
                    tx * ty * p11.g
                );
                blended.b = static_cast<uint8_t>(
                    (1.0f - tx) * (1.0f - ty) * p00.b +
                    tx * (1.0f - ty) * p10.b +
                    (1.0f - tx) * ty * p01.b +
                    tx * ty * p11.b
                );
                blended.a = 255;

                img.set_pixel(dx, dy, blended);
            }
        }
    }
}

// ---------------------------------------------------------
// Kuwahara Filter Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_kuwahara_filter(Image& img, int window_size) {
    int w = img.get_width();
    int h = img.get_height();
    Image src = img;

    if (window_size % 2 == 0) window_size++;
    int r = window_size / 2;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            struct Region {
                float sum_r = 0, sum_g = 0, sum_b = 0;
                float sum_sq_l = 0;
                float sum_l = 0;
                int count = 0;
            } regions[4];

            auto add_pixel = [&](int rx, int ry, int reg_idx) {
                int px = clamp(static_cast<float>(rx), 0.0f, static_cast<float>(w - 1));
                int py = clamp(static_cast<float>(ry), 0.0f, static_cast<float>(h - 1));
                Pixel p = src.get_pixel(px, py);
                
                regions[reg_idx].sum_r += p.r;
                regions[reg_idx].sum_g += p.g;
                regions[reg_idx].sum_b += p.b;
                
                float l = 0.299f * p.r + 0.587f * p.g + 0.114f * p.b;
                regions[reg_idx].sum_l += l;
                regions[reg_idx].sum_sq_l += l * l;
                regions[reg_idx].count++;
            };

            for (int dy = -r; dy <= 0; ++dy) {
                for (int dx = -r; dx <= 0; ++dx) {
                    add_pixel(x + dx, y + dy, 0);
                }
            }
            for (int dy = -r; dy <= 0; ++dy) {
                for (int dx = 0; dx <= r; ++dx) {
                    add_pixel(x + dx, y + dy, 1);
                }
            }
            for (int dy = 0; dy <= r; ++dy) {
                for (int dx = -r; dx <= 0; ++dx) {
                    add_pixel(x + dx, y + dy, 2);
                }
            }
            for (int dy = 0; dy <= r; ++dy) {
                for (int dx = 0; dx <= r; ++dx) {
                    add_pixel(x + dx, y + dy, 3);
                }
            }

            float min_var = -1.0f;
            int best_region = 0;

            for (int i = 0; i < 4; ++i) {
                float n = regions[i].count;
                if (n <= 1) continue;
                
                float var = (regions[i].sum_sq_l - (regions[i].sum_l * regions[i].sum_l) / n) / (n - 1);
                if (min_var < 0.0f || var < min_var) {
                    min_var = var;
                    best_region = i;
                }
            }

            Pixel out;
            float n = regions[best_region].count;
            out.r = static_cast<uint8_t>(clamp(regions[best_region].sum_r / n, 0.0f, 255.0f));
            out.g = static_cast<uint8_t>(clamp(regions[best_region].sum_g / n, 0.0f, 255.0f));
            out.b = static_cast<uint8_t>(clamp(regions[best_region].sum_b / n, 0.0f, 255.0f));
            out.a = 255;
            img.set_pixel(x, y, out);
        }
    }
}

// ---------------------------------------------------------
// Seam Carving Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::seam_carve_width(Image& img, int target_width) {
    int w = img.get_width();
    int h = img.get_height();
    if (target_width >= w || target_width <= 0) return;

    int num_seams_to_remove = w - target_width;

    for (int seam_idx = 0; seam_idx < num_seams_to_remove; ++seam_idx) {
        int cur_w = img.get_width();
        
        std::vector<float> energy(cur_w * h, 0.0f);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < cur_w; ++x) {
                int x_left = (x > 0) ? x - 1 : x;
                int x_right = (x < cur_w - 1) ? x + 1 : x;
                int y_top = (y > 0) ? y - 1 : y;
                int y_bottom = (y < h - 1) ? y + 1 : y;

                Pixel p_l = img.get_pixel(x_left, y);
                Pixel p_r = img.get_pixel(x_right, y);
                Pixel p_t = img.get_pixel(x, y_top);
                Pixel p_b = img.get_pixel(x, y_bottom);

                float dx_r = p_r.r - p_l.r;
                float dx_g = p_r.g - p_l.g;
                float dx_b = p_r.b - p_l.b;

                float dy_r = p_b.r - p_t.r;
                float dy_g = p_b.g - p_t.g;
                float dy_b = p_b.b - p_t.b;

                energy[y * cur_w + x] = std::sqrt(dx_r*dx_r + dx_g*dx_g + dx_b*dx_b + dy_r*dy_r + dy_g*dy_g + dy_b*dy_b);
            }
        }

        std::vector<float> dp(cur_w * h, 0.0f);
        for (int x = 0; x < cur_w; ++x) {
            dp[x] = energy[x];
        }

        for (int y = 1; y < h; ++y) {
            for (int x = 0; x < cur_w; ++x) {
                float e = energy[y * cur_w + x];
                float m = dp[(y - 1) * cur_w + x];
                if (x > 0) {
                    m = std::min(m, dp[(y - 1) * cur_w + x - 1]);
                }
                if (x < cur_w - 1) {
                    m = std::min(m, dp[(y - 1) * cur_w + x + 1]);
                }
                dp[y * cur_w + x] = e + m;
            }
        }

        std::vector<int> seam(h);
        int min_x = 0;
        float min_val = dp[(h - 1) * cur_w];
        for (int x = 1; x < cur_w; ++x) {
            float val = dp[(h - 1) * cur_w + x];
            if (val < min_val) {
                min_val = val;
                min_x = x;
            }
        }
        seam[h - 1] = min_x;

        for (int y = h - 2; y >= 0; --y) {
            int prev_x = seam[y + 1];
            int best_x = prev_x;
            float best_val = dp[y * cur_w + prev_x];

            if (prev_x > 0) {
                float val = dp[y * cur_w + prev_x - 1];
                if (val < best_val) {
                    best_val = val;
                    best_x = prev_x - 1;
                }
            }
            if (prev_x < cur_w - 1) {
                float val = dp[y * cur_w + prev_x + 1];
                if (val < best_val) {
                    best_val = val;
                    best_x = prev_x + 1;
                }
            }
            seam[y] = best_x;
        }

        Image next_img(cur_w - 1, h);
        for (int y = 0; y < h; ++y) {
            int seam_x = seam[y];
            for (int x = 0; x < seam_x; ++x) {
                next_img.set_pixel(x, y, img.get_pixel(x, y));
            }
            for (int x = seam_x + 1; x < cur_w; ++x) {
                next_img.set_pixel(x - 1, y, img.get_pixel(x, y));
            }
        }
        img = next_img;
    }
}

// ---------------------------------------------------------
// Harris Corner Detection Implementation
// ---------------------------------------------------------

std::vector<Point2D> ImageProcessorExtra::detect_harris_corners(Image& img, float k, float threshold, bool draw_corners) {
    int w = img.get_width();
    int h = img.get_height();
    
    std::vector<float> Ix(w * h, 0.0f);
    std::vector<float> Iy(w * h, 0.0f);

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float gx = 
                -1.0f * (0.299f * img.get_pixel(x-1, y-1).r + 0.587f * img.get_pixel(x-1, y-1).g + 0.114f * img.get_pixel(x-1, y-1).b) +
                 1.0f * (0.299f * img.get_pixel(x+1, y-1).r + 0.587f * img.get_pixel(x+1, y-1).g + 0.114f * img.get_pixel(x+1, y-1).b) +
                -2.0f * (0.299f * img.get_pixel(x-1, y).r   + 0.587f * img.get_pixel(x-1, y).g   + 0.114f * img.get_pixel(x-1, y).b) +
                 2.0f * (0.299f * img.get_pixel(x+1, y).r   + 0.587f * img.get_pixel(x+1, y).g   + 0.114f * img.get_pixel(x+1, y).b) +
                -1.0f * (0.299f * img.get_pixel(x-1, y+1).r + 0.587f * img.get_pixel(x-1, y+1).g + 0.114f * img.get_pixel(x-1, y+1).b) +
                 1.0f * (0.299f * img.get_pixel(x+1, y+1).r + 0.587f * img.get_pixel(x+1, y+1).g + 0.114f * img.get_pixel(x+1, y+1).b);

            float gy = 
                -1.0f * (0.299f * img.get_pixel(x-1, y-1).r + 0.587f * img.get_pixel(x-1, y-1).g + 0.114f * img.get_pixel(x-1, y-1).b) +
                -2.0f * (0.299f * img.get_pixel(x, y-1).r   + 0.587f * img.get_pixel(x, y-1).g   + 0.114f * img.get_pixel(x, y-1).b) +
                -1.0f * (0.299f * img.get_pixel(x+1, y-1).r + 0.587f * img.get_pixel(x+1, y-1).g + 0.114f * img.get_pixel(x+1, y-1).b) +
                 1.0f * (0.299f * img.get_pixel(x-1, y+1).r + 0.587f * img.get_pixel(x-1, y+1).g + 0.114f * img.get_pixel(x-1, y+1).b) +
                 2.0f * (0.299f * img.get_pixel(x, y+1).r   + 0.587f * img.get_pixel(x, y+1).g   + 0.114f * img.get_pixel(x, y+1).b) +
                 1.0f * (0.299f * img.get_pixel(x+1, y+1).r + 0.587f * img.get_pixel(x+1, y+1).g + 0.114f * img.get_pixel(x+1, y+1).b);

            Ix[y * w + x] = gx;
            Iy[y * w + x] = gy;
        }
    }

    std::vector<float> A(w * h, 0.0f);
    std::vector<float> B(w * h, 0.0f);
    std::vector<float> C_mat(w * h, 0.0f);

    for (int i = 0; i < w * h; ++i) {
        A[i] = Ix[i] * Ix[i];
        B[i] = Iy[i] * Iy[i];
        C_mat[i] = Ix[i] * Iy[i];
    }

    std::vector<float> A_smooth(w * h, 0.0f);
    std::vector<float> B_smooth(w * h, 0.0f);
    std::vector<float> C_smooth(w * h, 0.0f);

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            float sum_A = 0, sum_B = 0, sum_C = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int idx = (y + dy) * w + (x + dx);
                    sum_A += A[idx];
                    sum_B += B[idx];
                    sum_C += C_mat[idx];
                }
            }
            A_smooth[y * w + x] = sum_A / 9.0f;
            B_smooth[y * w + x] = sum_B / 9.0f;
            C_smooth[y * w + x] = sum_C / 9.0f;
        }
    }

    std::vector<float> R(w * h, 0.0f);
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int idx = y * w + x;
            float a = A_smooth[idx];
            float b = B_smooth[idx];
            float c = C_smooth[idx];

            float det = a * b - c * c;
            float trace = a + b;
            R[idx] = det - k * trace * trace;
        }
    }

    std::vector<Point2D> corners;
    for (int y = 2; y < h - 2; ++y) {
        for (int x = 2; x < w - 2; ++x) {
            int idx = y * w + x;
            float val = R[idx];
            if (val > threshold) {
                bool is_max = true;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (R[(y + dy) * w + (x + dx)] > val) {
                            is_max = false;
                            break;
                        }
                    }
                    if (!is_max) break;
                }
                if (is_max) {
                    corners.push_back({static_cast<float>(x), static_cast<float>(y)});
                }
            }
        }
    }

    if (draw_corners) {
        Pixel red_color{255, 0, 0, 255};
        for (const auto& pt : corners) {
            int cx = static_cast<int>(pt.x);
            int cy = static_cast<int>(pt.y);
            for (int dx = -3; dx <= 3; ++dx) {
                if (cx + dx >= 0 && cx + dx < w) {
                    img.set_pixel(cx + dx, cy, red_color);
                }
            }
            for (int dy = -3; dy <= 3; ++dy) {
                if (cy + dy >= 0 && cy + dy < h) {
                    img.set_pixel(cx, cy + dy, red_color);
                }
            }
        }
    }

    return corners;
}

// ---------------------------------------------------------
// Image Segmentation Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_k_means_segmentation(Image& img, int k_clusters, int max_iterations) {
    int w = img.get_width();
    int h = img.get_height();
    if (k_clusters <= 0 || w * h == 0) return;

    struct PointRGB {
        float r, g, b;
    };
    std::vector<PointRGB> points(w * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Pixel p = img.get_pixel(x, y);
            points[y * w + x] = {static_cast<float>(p.r), static_cast<float>(p.g), static_cast<float>(p.b)};
        }
    }

    std::vector<PointRGB> centroids(k_clusters);
    int step = (w * h) / k_clusters;
    for (int i = 0; i < k_clusters; ++i) {
        centroids[i] = points[std::min(i * step + step / 2, w * h - 1)];
    }

    std::vector<int> assignments(w * h, 0);

    for (int iter = 0; iter < max_iterations; ++iter) {
        for (int i = 0; i < w * h; ++i) {
            float min_dist_sq = -1.0f;
            int best_centroid = 0;
            for (int c = 0; c < k_clusters; ++c) {
                float dr = points[i].r - centroids[c].r;
                float dg = points[i].g - centroids[c].g;
                float db = points[i].b - centroids[c].b;
                float dist_sq = dr * dr + dg * dg + db * db;
                if (min_dist_sq < 0.0f || dist_sq < min_dist_sq) {
                    min_dist_sq = dist_sq;
                    best_centroid = c;
                }
            }
            assignments[i] = best_centroid;
        }

        std::vector<PointRGB> new_centroids(k_clusters, {0.0f, 0.0f, 0.0f});
        std::vector<int> counts(k_clusters, 0);

        for (int i = 0; i < w * h; ++i) {
            int c = assignments[i];
            new_centroids[c].r += points[i].r;
            new_centroids[c].g += points[i].g;
            new_centroids[c].b += points[i].b;
            counts[c]++;
        }

        bool changed = false;
        for (int c = 0; c < k_clusters; ++c) {
            if (counts[c] > 0) {
                PointRGB updated = {
                    new_centroids[c].r / counts[c],
                    new_centroids[c].g / counts[c],
                    new_centroids[c].b / counts[c]
                };
                float dr = updated.r - centroids[c].r;
                float dg = updated.g - centroids[c].g;
                float db = updated.b - centroids[c].b;
                if (dr * dr + dg * dg + db * db > 0.1f) {
                    changed = true;
                }
                centroids[c] = updated;
            }
        }

        if (!changed) break;
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            int c = assignments[idx];
            Pixel p = img.get_pixel(x, y);
            p.r = static_cast<uint8_t>(clamp(centroids[c].r, 0.0f, 255.0f));
            p.g = static_cast<uint8_t>(clamp(centroids[c].g, 0.0f, 255.0f));
            p.b = static_cast<uint8_t>(clamp(centroids[c].b, 0.0f, 255.0f));
            img.set_pixel(x, y, p);
        }
    }
}

// ---------------------------------------------------------
// Morphological Operations Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_dilation(Image& img, int radius) {
    int w = img.get_width();
    int h = img.get_height();
    Image src = img;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t max_r = 0, max_g = 0, max_b = 0;
            for (int dy = -radius; dy <= radius; ++dy) {
                int py = clamp(static_cast<float>(y + dy), 0.0f, static_cast<float>(h - 1));
                for (int dx = -radius; dx <= radius; ++dx) {
                    int px = clamp(static_cast<float>(x + dx), 0.0f, static_cast<float>(w - 1));
                    Pixel p = src.get_pixel(px, py);
                    if (p.r > max_r) max_r = p.r;
                    if (p.g > max_g) max_g = p.g;
                    if (p.b > max_b) max_b = p.b;
                }
            }
            Pixel p = src.get_pixel(x, y);
            p.r = max_r;
            p.g = max_g;
            p.b = max_b;
            img.set_pixel(x, y, p);
        }
    }
}

void ImageProcessorExtra::apply_erosion(Image& img, int radius) {
    int w = img.get_width();
    int h = img.get_height();
    Image src = img;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t min_r = 255, min_g = 255, min_b = 255;
            for (int dy = -radius; dy <= radius; ++dy) {
                int py = clamp(static_cast<float>(y + dy), 0.0f, static_cast<float>(h - 1));
                for (int dx = -radius; dx <= radius; ++dx) {
                    int px = clamp(static_cast<float>(x + dx), 0.0f, static_cast<float>(w - 1));
                    Pixel p = src.get_pixel(px, py);
                    if (p.r < min_r) min_r = p.r;
                    if (p.g < min_g) min_g = p.g;
                    if (p.b < min_b) min_b = p.b;
                }
            }
            Pixel p = src.get_pixel(x, y);
            p.r = min_r;
            p.g = min_g;
            p.b = min_b;
            img.set_pixel(x, y, p);
        }
    }
}

// ---------------------------------------------------------
// Integral Image & Fast Filtering Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_fast_box_blur(Image& img, int radius) {
    int w = img.get_width();
    int h = img.get_height();
    if (w * h == 0 || radius <= 0) return;

    std::vector<uint64_t> int_r(w * h, 0);
    std::vector<uint64_t> int_g(w * h, 0);
    std::vector<uint64_t> int_b(w * h, 0);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Pixel p = img.get_pixel(x, y);
            
            uint64_t r_val = p.r;
            uint64_t g_val = p.g;
            uint64_t b_val = p.b;

            uint64_t left_r = (x > 0) ? int_r[y * w + (x - 1)] : 0;
            uint64_t top_r = (y > 0) ? int_r[(y - 1) * w + x] : 0;
            uint64_t topleft_r = (x > 0 && y > 0) ? int_r[(y - 1) * w + (x - 1)] : 0;

            uint64_t left_g = (x > 0) ? int_g[y * w + (x - 1)] : 0;
            uint64_t top_g = (y > 0) ? int_g[(y - 1) * w + x] : 0;
            uint64_t topleft_g = (x > 0 && y > 0) ? int_g[(y - 1) * w + (x - 1)] : 0;

            uint64_t left_b = (x > 0) ? int_b[y * w + (x - 1)] : 0;
            uint64_t top_b = (y > 0) ? int_b[(y - 1) * w + x] : 0;
            uint64_t topleft_b = (x > 0 && y > 0) ? int_b[(y - 1) * w + (x - 1)] : 0;

            int_r[y * w + x] = r_val + left_r + top_r - topleft_r;
            int_g[y * w + x] = g_val + left_g + top_g - topleft_g;
            int_b[y * w + x] = b_val + left_b + top_b - topleft_b;
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int x1 = std::max(0, x - radius);
            int x2 = std::min(w - 1, x + radius);
            int y1 = std::max(0, y - radius);
            int y2 = std::min(h - 1, y + radius);

            auto get_area_sum = [](const std::vector<uint64_t>& integral, int w, int x1, int y1, int x2, int y2) -> uint64_t {
                uint64_t br = integral[y2 * w + x2];
                uint64_t tr = (y1 > 0) ? integral[(y1 - 1) * w + x2] : 0;
                uint64_t bl = (x1 > 0) ? integral[y2 * w + (x1 - 1)] : 0;
                uint64_t tl = (x1 > 0 && y1 > 0) ? integral[(y1 - 1) * w + (x1 - 1)] : 0;
                return br - tr - bl + tl;
            };

            uint64_t sum_r = get_area_sum(int_r, w, x1, y1, x2, y2);
            uint64_t sum_g = get_area_sum(int_g, w, x1, y1, x2, y2);
            uint64_t sum_b = get_area_sum(int_b, w, x1, y1, x2, y2);

            float area = (x2 - x1 + 1) * (y2 - y1 + 1);

            Pixel p = img.get_pixel(x, y);
            p.r = static_cast<uint8_t>(clamp(sum_r / area, 0.0f, 255.0f));
            p.g = static_cast<uint8_t>(clamp(sum_g / area, 0.0f, 255.0f));
            p.b = static_cast<uint8_t>(clamp(sum_b / area, 0.0f, 255.0f));
            img.set_pixel(x, y, p);
        }
    }
}

// ---------------------------------------------------------
// Texture Analysis Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_local_binary_patterns(Image& img) {
    int w = img.get_width();
    int h = img.get_height();
    Image src = img;

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            Pixel center_p = src.get_pixel(x, y);
            float center_val = 0.299f * center_p.r + 0.587f * center_p.g + 0.114f * center_p.b;

            uint8_t lbp_code = 0;
            int neighbors_x[8] = {x-1, x, x+1, x+1, x+1, x, x-1, x-1};
            int neighbors_y[8] = {y-1, y-1, y-1, y, y+1, y+1, y+1, y};

            for (int i = 0; i < 8; ++i) {
                Pixel neighbor_p = src.get_pixel(neighbors_x[i], neighbors_y[i]);
                float neighbor_val = 0.299f * neighbor_p.r + 0.587f * neighbor_p.g + 0.114f * neighbor_p.b;
                
                if (neighbor_val >= center_val) {
                    lbp_code |= (1 << i);
                }
            }

            Pixel out{lbp_code, lbp_code, lbp_code, 255};
            img.set_pixel(x, y, out);
        }
    }
}

// ---------------------------------------------------------
// Multi-level Thresholding Implementation
// ---------------------------------------------------------

void ImageProcessorExtra::apply_multilevel_otsu_thresholding(Image& img) {
    int w = img.get_width();
    int h = img.get_height();
    if (w * h == 0) return;

    std::vector<int> hist(256, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Pixel p = img.get_pixel(x, y);
            int val = static_cast<int>(std::round(0.299f * p.r + 0.587f * p.g + 0.114f * p.b));
            val = std::max(0, std::min(val, 255));
            hist[val]++;
        }
    }

    float total = w * h;
    std::vector<float> p_prob(256, 0.0f);
    for (int i = 0; i < 256; ++i) {
        p_prob[i] = hist[i] / total;
    }

    float mu_T = 0.0f;
    for (int i = 0; i < 256; ++i) {
        mu_T += i * p_prob[i];
    }

    float max_var = -1.0f;
    int best_t1 = 0;
    int best_t2 = 127;

    for (int t1 = 0; t1 < 254; ++t1) {
        float w0 = 0.0f;
        float u0_sum = 0.0f;
        for (int i = 0; i <= t1; ++i) {
            w0 += p_prob[i];
            u0_sum += i * p_prob[i];
        }

        if (w0 == 0.0f) continue;

        for (int t2 = t1 + 1; t2 < 255; ++t2) {
            float w1 = 0.0f;
            float u1_sum = 0.0f;
            for (int i = t1 + 1; i <= t2; ++i) {
                w1 += p_prob[i];
                u1_sum += i * p_prob[i];
            }

            float w2 = 1.0f - w0 - w1;
            if (w1 == 0.0f || w2 <= 0.0f) continue;

            float u0 = u0_sum / w0;
            float u1 = u1_sum / w1;

            float u2_sum = 0.0f;
            for (int i = t2 + 1; i < 256; ++i) {
                u2_sum += i * p_prob[i];
            }
            float u2 = u2_sum / w2;

            float var = w0 * (u0 - mu_T) * (u0 - mu_T) +
                        w1 * (u1 - mu_T) * (u1 - mu_T) +
                        w2 * (u2 - mu_T) * (u2 - mu_T);

            if (var > max_var) {
                max_var = var;
                best_t1 = t1;
                best_t2 = t2;
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Pixel p = img.get_pixel(x, y);
            int val = static_cast<int>(std::round(0.299f * p.r + 0.587f * p.g + 0.114f * p.b));
            
            uint8_t out_val = 0;
            if (val <= best_t1) {
                out_val = 0;
            } else if (val <= best_t2) {
                out_val = 127;
            } else {
                out_val = 255;
            }

            p.r = out_val;
            p.g = out_val;
            p.b = out_val;
            img.set_pixel(x, y, p);
        }
    }
}

} // namespace PixelForge
