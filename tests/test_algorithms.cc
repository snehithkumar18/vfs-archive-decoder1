#include "../src/image.h"
#include "../src/math_utils.h"
#include "../src/convolution.h"
#include "../src/transform.h"
#include "../src/histogram.h"
#include "../src/drawing.h"
#include "../src/analysis.h"
#include "../src/effects.h"
#include "../src/enhancement.h"
#include "../src/thread_pool.h"
#include "../src/logger.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <random>
#include <future>
#include <chrono>

using namespace PixelForge;

// Generate test image with random noise
Image generate_noisy_image(uint32_t w, uint32_t h, uint32_t seed = 42) {
    Image img(w, h, PixelFormat::RGB888);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(0, 255);

    auto& data = img.getData();
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(dis(gen));
    }
    img.sync();
    return img;
}

// Generate test bimodal image (for Otsu testing)
Image generate_bimodal_image(uint32_t w, uint32_t h) {
    Image img(w, h, PixelFormat::Grayscale);
    auto& data = img.getData();
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = static_cast<size_t>(y) * w + x;
            if ((x + y) % 2 == 0) {
                data[idx] = 40; // Peak 1
            } else {
                data[idx] = 200; // Peak 2
            }
        }
    }
    img.sync();
    return img;
}

void test_color_conversions_extreme() {
    std::cout << "Testing extreme color conversions...\n";
    // Check black, white, primary, and secondary colors
    std::vector<std::vector<uint8_t>> test_colors = {
        {0, 0, 0},
        {255, 255, 255},
        {255, 0, 0},
        {0, 255, 0},
        {0, 0, 255},
        {255, 255, 0},
        {0, 255, 255},
        {255, 0, 255},
        {128, 128, 128},
        {10, 80, 200}
    };

    for (const auto& color : test_colors) {
        uint8_t r = color[0], g = color[1], b = color[2];

        // HSV
        HSVColor hsv = RGBToHSV(r, g, b);
        uint8_t r_hsv, g_hsv, b_hsv;
        HSVToRGB(hsv, r_hsv, g_hsv, b_hsv);
        assert(std::abs(r - r_hsv) <= 2);
        assert(std::abs(g - g_hsv) <= 2);
        assert(std::abs(b - b_hsv) <= 2);

        // HSL
        HSLColor hsl = RGBToHSL(r, g, b);
        uint8_t r_hsl, g_hsl, b_hsl;
        HSLToRGB(hsl, r_hsl, g_hsl, b_hsl);
        assert(std::abs(r - r_hsl) <= 2);
        assert(std::abs(g - g_hsl) <= 2);
        assert(std::abs(b - b_hsl) <= 2);

        // YCbCr
        YCbCrColor ycc = RGBToYCbCr(r, g, b);
        uint8_t r_ycc, g_ycc, b_ycc;
        YCbCrToRGB(ycc, r_ycc, g_ycc, b_ycc);
        assert(std::abs(r - r_ycc) <= 2);
        assert(std::abs(g - g_ycc) <= 2);
        assert(std::abs(b - b_ycc) <= 2);

        // XYZ & Lab
        XYZColor xyz = RGBToXYZ(r, g, b);
        LabColor lab = XYZToLab(xyz);
        XYZColor xyz2 = LabToXYZ(lab);
        uint8_t r_lab, g_lab, b_lab;
        XYZToRGB(xyz2, r_lab, g_lab, b_lab);
        assert(std::abs(r - r_lab) <= 3);
        assert(std::abs(g - g_lab) <= 3);
        assert(std::abs(b - b_lab) <= 3);
    }
}

void test_otsu_threshold_accuracy() {
    std::cout << "Testing Otsu thresholding accuracy...\n";
    Image img = generate_bimodal_image(64, 64);
    uint8_t threshold = Analysis::ComputeOtsuThreshold(img);
    // The peaks are at 40 and 200. Optimal threshold should be around (40 + 200) / 2 = 120
    assert(threshold > 50 && threshold < 190);

    Image binarized;
    PixelForgeErrorCode err = Analysis::Threshold(img, binarized, threshold);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(binarized.getFormat() == PixelFormat::Grayscale);

    // Verify binarization outputs strictly 0 or 255
    const auto& data = binarized.getData();
    for (uint8_t val : data) {
        assert(val == 0 || val == 255);
    }
}

void test_connected_components_complex() {
    std::cout << "Testing CCL on complex shapes...\n";
    // Create a 64x64 binary image with 4 distinct objects
    Image img(64, 64, PixelFormat::Grayscale);
    std::memset(img.getData().data(), 0, img.getData().size());

    // Object 1: Rectangle [5..15, 5..15]
    Drawing::DrawRect(img, 5, 5, 10, 10, ColorRGBA(255, 255, 255), true);
    // Object 2: Circle at (40, 15), radius 6
    Drawing::DrawCircle(img, 40, 15, 6, ColorRGBA(255, 255, 255), true);
    // Object 3: Line from (5, 45) to (25, 45)
    Drawing::DrawLine(img, 5, 45, 25, 45, ColorRGBA(255, 255, 255));
    // Object 4: Line from (40, 45) to (60, 55)
    Drawing::DrawLine(img, 40, 45, 60, 55, ColorRGBA(255, 255, 255));

    img.sync();

    Image labeled;
    uint32_t num_components = 0;
    PixelForgeErrorCode err = Analysis::LabelComponents(img, labeled, num_components);
    assert(err == PixelForgeErrorCode::SUCCESS);
    // Should detect exactly 4 components
    assert(num_components == 4);
    assert(labeled.getWidth() == 64);
    assert(labeled.getFormat() == PixelFormat::RGB888);
}

void test_convolution_kernels() {
    std::cout << "Testing custom convolution kernels...\n";
    Image img = generate_noisy_image(32, 32);
    Image dst;

    // Test Gaussian Blur 5x5
    PixelForgeErrorCode err = Convolution::Apply(img, dst, Convolution::GetGaussianBlur5x5());
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32);

    // Test Box Blur 3x3
    err = Convolution::Apply(img, dst, Convolution::GetBoxBlur3x3());
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Test Sobel filters
    err = Convolution::Apply(img, dst, Convolution::GetSobelX3x3());
    assert(err == PixelForgeErrorCode::SUCCESS);
    err = Convolution::Apply(img, dst, Convolution::GetSobelY3x3());
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Test Laplacian
    err = Convolution::Apply(img, dst, Convolution::GetLaplacian3x3());
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Test custom kernel (Identity)
    Kernel identity(3, 3, {0, 0, 0, 0, 1, 0, 0, 0, 0});
    err = Convolution::Apply(img, dst, identity);
    assert(err == PixelForgeErrorCode::SUCCESS);
    
    // Identity filter output should match source exactly
    const auto& src_data = img.getData();
    const auto& dst_data = dst.getData();
    assert(src_data == dst_data);
}

void test_transforms_interpolation() {
    std::cout << "Testing rotation interpolation and bounds...\n";
    Image img(32, 32, PixelFormat::RGB888);
    // Draw crosshair to check rotational features
    Drawing::DrawLine(img, 0, 16, 31, 16, ColorRGBA(255, 0, 0));
    Drawing::DrawLine(img, 16, 0, 16, 31, ColorRGBA(0, 255, 0));
    img.sync();

    Image dst;
    // Rotate 45 degrees
    PixelForgeErrorCode err = Transform::Rotate(img, dst, 45.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);
    // Due to bounding box increase, width/height should have grown
    assert(dst.getWidth() > 32);
    assert(dst.getHeight() > 32);

    // Rotate 90 degrees (fast method)
    Image r90;
    err = Transform::Rotate90(img, r90);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(r90.getWidth() == 32 && r90.getHeight() == 32);

    // Rotate 180 degrees
    Image r180;
    err = Transform::Rotate180(img, r180);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Rotate 270 degrees
    Image r270;
    err = Transform::Rotate270(img, r270);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

void test_ssim_accuracy() {
    std::cout << "Testing SSIM similarity index accuracy...\n";
    Image img1 = generate_noisy_image(32, 32, 100);
    Image img2 = img1; // identical copy

    float ssim_same = 0.0f;
    PixelForgeErrorCode err = Analysis::ComputeSSIM(img1, img2, ssim_same);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(std::abs(ssim_same - 1.0f) < 0.001f); // Should be exactly 1.0

    // Slightly blurred image
    Image blurred;
    err = Convolution::Apply(img1, blurred, Convolution::GetBoxBlur3x3());
    assert(err == PixelForgeErrorCode::SUCCESS);

    float ssim_diff = 0.0f;
    err = Analysis::ComputeSSIM(img1, blurred, ssim_diff);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(ssim_diff < 1.0f && ssim_diff > 0.0f); // Should be between 0 and 1
}

void test_effects_grading() {
    std::cout << "Testing image effects and grading...\n";
    Image img = generate_noisy_image(32, 32);
    Image dst;

    // Brightness contrast adjustments
    PixelForgeErrorCode err = Effects::AdjustBrightnessContrast(img, dst, 20.0f, -10.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Gamma correction
    err = Effects::GammaCorrection(img, dst, 2.2f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Sepia filter
    err = Effects::Sepia(img, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Vignette
    err = Effects::Vignette(img, dst, 0.7f, 1.5f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Color blindness
    err = Effects::SimulateColorBlindness(img, dst, ColorBlindType::DEUTERANOPIA);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

void test_thread_pool_parallel_filtering() {
    std::cout << "Testing parallelized image filtering...\n";
    ThreadPool pool(4);
    
    // Load 10 images
    std::vector<Image> images;
    for (int i = 0; i < 10; ++i) {
        images.push_back(generate_noisy_image(64, 64, 1000 + i));
    }

    std::vector<std::future<Image>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.Submit(1, [img = images[i]]() {
            Image blurred;
            Convolution::Apply(img, blurred, Convolution::GetBoxBlur3x3());
            
            Image final_img;
            Effects::AdjustBrightnessContrast(blurred, final_img, 10.0f, 5.0f);
            return final_img;
        }));
    }

    // Wait for all filtering tasks to complete in parallel
    for (int i = 0; i < 10; ++i) {
        Image out = futures[i].get();
        assert(out.getWidth() == 64);
        assert(out.getHeight() == 64);
        assert(out.getFormat() == PixelFormat::RGB888);
    }
}

void test_enhancement() {
    std::cout << "Testing image enhancement filters...\n";
    Image img(32, 32, PixelFormat::RGB888);
    // Draw diagonal cross to check details
    Drawing::DrawLine(img, 0, 0, 31, 31, ColorRGBA(255, 255, 255));
    img.sync();

    Image dst;

    // Median filter
    PixelForgeErrorCode err = Enhancement::MedianFilter(img, dst, 3);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32);

    // Bilateral filter
    err = Enhancement::BilateralFilter(img, dst, 2.0f, 0.1f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Unsharp mask
    err = Enhancement::UnsharpMask(img, dst, 1.2f, 2.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Histogram matching
    Image ref = generate_noisy_image(32, 32, 777);
    err = Enhancement::MatchHistogram(img, ref, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Adaptive histogram equalization
    err = Enhancement::AdaptiveHistogramEqualization(img, dst, 16);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

void test_drawing_extended() {
    std::cout << "Testing extended drawing shapes...\n";
    Image img(64, 64, PixelFormat::RGBA8888);
    ColorRGBA color(255, 0, 255, 255);

    // Ellipse
    PixelForgeErrorCode err = Drawing::DrawEllipse(img, 32, 32, 20, 10, color, false);
    assert(err == PixelForgeErrorCode::SUCCESS);
    err = Drawing::DrawEllipse(img, 32, 32, 10, 5, color, true);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Polygon
    std::vector<Point2D> poly = {
        Point2D(10, 10),
        Point2D(30, 5),
        Point2D(50, 15),
        Point2D(40, 45),
        Point2D(15, 35)
    };
    err = Drawing::DrawPolygon(img, poly, color, false);
    assert(err == PixelForgeErrorCode::SUCCESS);
    err = Drawing::DrawPolygon(img, poly, color, true);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Bezier Curves
    err = Drawing::DrawBezierQuadratic(img, Point2D(0, 0), Point2D(32, 60), Point2D(63, 0), color);
    assert(err == PixelForgeErrorCode::SUCCESS);

    err = Drawing::DrawBezierCubic(img, Point2D(0, 63), Point2D(20, 0), Point2D(40, 0), Point2D(63, 63), color);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

int main() {
    std::cout << "Starting PixelForge Algorithm & Pipeline Tests...\n";
    
    test_color_conversions_extreme();
    test_otsu_threshold_accuracy();
    test_connected_components_complex();
    test_convolution_kernels();
    test_transforms_interpolation();
    test_ssim_accuracy();
    test_effects_grading();
    test_enhancement();
    test_drawing_extended();
    test_thread_pool_parallel_filtering();

    std::cout << "All PixelForge Algorithm & Pipeline Tests Passed Successfully!\n";
    return 0;
}
