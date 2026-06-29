#ifndef PIXELFORGE_IMAGE_PROCESSOR_EXTRA_H
#define PIXELFORGE_IMAGE_PROCESSOR_EXTRA_H

#include "image.h"
#include <vector>
#include <string>

namespace PixelForge {

struct Point2D {
    float x;
    float y;
};

struct Point3D {
    float x;
    float y;
    float z;
};

struct Matrix3D {
    float m[4][4];
};

struct ColorHSL {
    float h; // 0-360
    float s; // 0-1
    float l; // 0-1
};

struct ColorHSV {
    float h; // 0-360
    float s; // 0-1
    float v; // 0-1
};

struct ColorYUV {
    float y;
    float u;
    float v;
};

struct ColorCMYK {
    float c;
    float m;
    float y;
    float k;
};

struct ColorLAB {
    float l;
    float a;
    float b;
};

class ImageProcessorExtra {
public:
    // Antialiased Drawing & Text
    static void draw_line_aa(Image& img, Point2D p1, Point2D p2, Pixel color);
    static void draw_circle_aa(Image& img, Point2D center, float radius, Pixel color);
    static void draw_bezier_quadratic(Image& img, Point2D p0, Point2D p1, Point2D p2, Pixel color);
    static void draw_bezier_cubic(Image& img, Point2D p0, Point2D p1, Point2D p2, Point2D p3, Pixel color);
    static void fill_polygon(Image& img, const std::vector<Point2D>& vertices, Pixel color);
    static void draw_text_aa(Image& img, int x, int y, const std::string& text, int scale, Pixel color);

    // Color Space Conversions
    static ColorHSL rgb_to_hsl(Pixel p);
    static Pixel hsl_to_rgb(ColorHSL hsl);
    static ColorHSV rgb_to_hsv(Pixel p);
    static Pixel hsv_to_rgb(ColorHSV hsv);
    static ColorYUV rgb_to_yuv(Pixel p);
    static Pixel yuv_to_rgb(ColorYUV yuv);
    static ColorCMYK rgb_to_cmyk(Pixel p);
    static Pixel cmyk_to_rgb(ColorCMYK cmyk);
    static ColorLAB rgb_to_lab(Pixel p);
    static Pixel lab_to_rgb(ColorLAB lab);

    // Advanced Filters & 3D LUT
    static void apply_vignette(Image& img, float radius, float softness);
    static void apply_bilateral_filter(Image& img, int radius, float sigma_d, float sigma_r);
    static void apply_adaptive_histogram_equalization(Image& img, int grid_size, float clip_limit);
    static void apply_3d_lut(Image& img, const std::vector<Pixel>& lut_table, int lut_size);
    static void apply_kuwahara_filter(Image& img, int window_size);

    // 3D Perspective Projection
    static void apply_3d_perspective_projection(Image& img, float yaw, float pitch, float roll, float fov, float distance);

    // Seam Carving
    static void seam_carve_width(Image& img, int target_width);

    // Feature Detection
    static std::vector<Point2D> detect_harris_corners(Image& img, float k, float threshold, bool draw_corners);

    // Image Segmentation
    static void apply_k_means_segmentation(Image& img, int k_clusters, int max_iterations);

    // Morphological Operations
    static void apply_dilation(Image& img, int radius);
    static void apply_erosion(Image& img, int radius);

    // Integral Image & Fast Filtering
    static void apply_fast_box_blur(Image& img, int radius);

    // Texture Analysis
    static void apply_local_binary_patterns(Image& img);

    // Multi-level Thresholding
    static void apply_multilevel_otsu_thresholding(Image& img);
};

} // namespace PixelForge

#endif // PIXELFORGE_IMAGE_PROCESSOR_EXTRA_H
