#include "../src/errors.h"
#include "../src/image.h"
#include "../src/image_processor_extra.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

using namespace PixelForge;

void test_color_conversions() {
    std::cout << "Testing color space conversions..." << std::endl;
    Pixel p{100, 150, 200, 255};

    // HSL
    ColorHSL hsl = ImageProcessorExtra::rgb_to_hsl(p);
    Pixel p_hsl = ImageProcessorExtra::hsl_to_rgb(hsl);
    assert(std::abs(p.r - p_hsl.r) <= 2);
    assert(std::abs(p.g - p_hsl.g) <= 2);
    assert(std::abs(p.b - p_hsl.b) <= 2);

    // HSV
    ColorHSV hsv = ImageProcessorExtra::rgb_to_hsv(p);
    Pixel p_hsv = ImageProcessorExtra::hsv_to_rgb(hsv);
    assert(std::abs(p.r - p_hsv.r) <= 2);
    assert(std::abs(p.g - p_hsv.g) <= 2);
    assert(std::abs(p.b - p_hsv.b) <= 2);

    // YUV
    ColorYUV yuv = ImageProcessorExtra::rgb_to_yuv(p);
    Pixel p_yuv = ImageProcessorExtra::yuv_to_rgb(yuv);
    assert(std::abs(p.r - p_yuv.r) <= 2);
    assert(std::abs(p.g - p_yuv.g) <= 2);
    assert(std::abs(p.b - p_yuv.b) <= 2);

    // CMYK
    ColorCMYK cmyk = ImageProcessorExtra::rgb_to_cmyk(p);
    Pixel p_cmyk = ImageProcessorExtra::cmyk_to_rgb(cmyk);
    assert(std::abs(p.r - p_cmyk.r) <= 2);
    assert(std::abs(p.g - p_cmyk.g) <= 2);
    assert(std::abs(p.b - p_cmyk.b) <= 2);

    // LAB
    ColorLAB lab = ImageProcessorExtra::rgb_to_lab(p);
    Pixel p_lab = ImageProcessorExtra::lab_to_rgb(lab);
    assert(std::abs(p.r - p_lab.r) <= 3);
    assert(std::abs(p.g - p_lab.g) <= 3);
    assert(std::abs(p.b - p_lab.b) <= 3);

    std::cout << "-> Color space conversions passed." << std::endl;
}

void test_drawing_aa() {
    std::cout << "Testing antialiased drawing..." << std::endl;
    Image img(50, 50, PixelFormat::RGB888);
    Pixel color{255, 0, 0, 255};

    // Draw Line AA
    ImageProcessorExtra::draw_line_aa(img, {0.0f, 0.0f}, {49.0f, 49.0f}, color);

    // Draw Circle AA
    ImageProcessorExtra::draw_circle_aa(img, {25.0f, 25.0f}, 10.0f, color);

    // Draw Quadratic Bezier
    ImageProcessorExtra::draw_bezier_quadratic(img, {0.0f, 0.0f}, {25.0f, 49.0f}, {49.0f, 0.0f}, color);

    // Draw Cubic Bezier
    ImageProcessorExtra::draw_bezier_cubic(img, {0.0f, 0.0f}, {10.0f, 40.0f}, {40.0f, 10.0f}, {49.0f, 49.0f}, color);

    // Fill Polygon
    std::vector<Point2D> poly = {{10.0f, 10.0f}, {40.0f, 15.0f}, {30.0f, 40.0f}};
    ImageProcessorExtra::fill_polygon(img, poly, color);

    // Draw Text AA
    ImageProcessorExtra::draw_text_aa(img, 2, 2, "HELLO*+", 1, color);

    std::cout << "-> Antialiased drawing passed." << std::endl;
}

void test_filters_and_transforms() {
    std::cout << "Testing advanced filters and transforms..." << std::endl;
    Image img(32, 32, PixelFormat::RGB888);
    for (uint32_t y = 0; y < 32; ++y) {
        for (uint32_t x = 0; x < 32; ++x) {
            uint8_t c = (x + y) * 4;
            uint8_t rgb[3] = {c, c, c};
            img.setPixel(x, y, rgb, 3);
        }
    }

    // Vignette
    ImageProcessorExtra::apply_vignette(img, 0.8f, 0.5f);

    // Bilateral Filter
    ImageProcessorExtra::apply_bilateral_filter(img, 2, 3.0f, 10.0f);

    // CLAHE
    ImageProcessorExtra::apply_adaptive_histogram_equalization(img, 4, 2.0f);

    // 3D LUT
    std::vector<Pixel> lut(16 * 16 * 16);
    for (int b = 0; b < 16; ++b) {
        for (int g = 0; g < 16; ++g) {
            for (int r = 0; r < 16; ++r) {
                int idx = r + g * 16 + b * 16 * 16;
                lut[idx] = {static_cast<uint8_t>(b * 17), static_cast<uint8_t>(g * 17), static_cast<uint8_t>(r * 17), 255};
            }
        }
    }
    ImageProcessorExtra::apply_3d_lut(img, lut, 16);

    // 3D Perspective Projection
    ImageProcessorExtra::apply_3d_perspective_projection(img, 10.0f, 15.0f, 5.0f, 60.0f, 1.5f);

    // Kuwahara Filter
    ImageProcessorExtra::apply_kuwahara_filter(img, 5);

    // Morphological dilation and erosion
    ImageProcessorExtra::apply_dilation(img, 2);
    ImageProcessorExtra::apply_erosion(img, 2);

    // Fast Box Blur
    ImageProcessorExtra::apply_fast_box_blur(img, 3);

    // Local Binary Patterns
    ImageProcessorExtra::apply_local_binary_patterns(img);

    // Multi-level Otsu thresholding
    ImageProcessorExtra::apply_multilevel_otsu_thresholding(img);

    std::cout << "-> Filters and transforms passed." << std::endl;
}

void test_seam_carving_and_harris() {
    std::cout << "Testing Seam Carving and Harris Corner Detection..." << std::endl;
    Image img(40, 40, PixelFormat::RGB888);
    for (uint32_t y = 0; y < 40; ++y) {
        for (uint32_t x = 0; x < 40; ++x) {
            bool is_inside = (x >= 10 && x <= 30 && y >= 10 && y <= 30);
            uint8_t val = is_inside ? 255 : 50;
            uint8_t rgb[3] = {val, val, val};
            img.setPixel(x, y, rgb, 3);
        }
    }

    // Harris Corner Detection
    auto corners = ImageProcessorExtra::detect_harris_corners(img, 0.04f, 10000.0f, true);
    assert(!corners.empty());

    // Seam Carving
    ImageProcessorExtra::seam_carve_width(img, 35);
    assert(img.getWidth() == 35);

    // K-Means Segmentation
    ImageProcessorExtra::apply_k_means_segmentation(img, 3, 10);

    std::cout << "-> Seam Carving, Harris Corners, and K-Means passed." << std::endl;
}

int main() {
    std::cout << "Running PixelForge Extra Feature Tests..." << std::endl;
    test_color_conversions();
    test_drawing_aa();
    test_filters_and_transforms();
    test_seam_carving_and_harris();
    std::cout << "All PixelForge Extra Feature Tests Passed Successfully!" << std::endl;
    return 0;
}
