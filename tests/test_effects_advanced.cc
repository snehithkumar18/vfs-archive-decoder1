#include "../src/effects_advanced.h"
#include "../src/image.h"
#include "../src/errors.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <chrono>
#include <set>
#include <random>

using namespace PixelForge;

// Generate a synthetic test image with a smooth color gradient
Image create_gradient_image(uint32_t w, uint32_t h) {
    Image img(w, h, PixelFormat::RGB888);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t r = static_cast<uint8_t>((x * 255) / w);
            uint8_t g = static_cast<uint8_t>((y * 255) / h);
            uint8_t b = static_cast<uint8_t>(((x + y) * 255) / (w + h));
            uint8_t pixel[3] = {r, g, b};
            img.setPixel(x, y, pixel, 3);
        }
    }
    return img;
}

// Generate a synthetic test image with transparency (RGBA)
Image create_rgba_test_image(uint32_t w, uint32_t h) {
    Image img(w, h, PixelFormat::RGBA8888);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t r = static_cast<uint8_t>((x * 255) / w);
            uint8_t g = static_cast<uint8_t>((y * 255) / h);
            uint8_t b = 128;
            uint8_t a = static_cast<uint8_t>((x * y * 255) / (w * h));
            uint8_t pixel[4] = {r, g, b, a};
            img.setPixel(x, y, pixel, 4);
        }
    }
    return img;
}

void test_floyd_steinberg_dither() {
    std::cout << "Running test_floyd_steinberg_dither..." << std::endl;
    Image src = create_gradient_image(64, 64);
    Image dst;

    // Test Monochrome Dither
    PixelForgeErrorCode err = AdvancedEffects::FloydSteinbergDither(src, dst, DitherPalette::MONOCHROME);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 64);
    assert(dst.getHeight() == 64);

    // Verify all pixels in monochrome dither are either strictly black or white
    const auto& data = dst.getData();
    for (size_t i = 0; i < data.size(); i += dst.getChannels()) {
        uint8_t r = data[i + 0];
        uint8_t g = data[i + 1];
        uint8_t b = data[i + 2];
        assert((r == 0 && g == 0 && b == 0) || (r == 255 && g == 255 && b == 255));
    }

    // Test CGA Dither
    err = AdvancedEffects::FloydSteinbergDither(src, dst, DitherPalette::CGA);
    assert(err == PixelForgeErrorCode::SUCCESS);
    
    // Test EGA Dither
    err = AdvancedEffects::FloydSteinbergDither(src, dst, DitherPalette::EGA);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Test Web-Safe Dither
    err = AdvancedEffects::FloydSteinbergDither(src, dst, DitherPalette::WEB_SAFE);
    assert(err == PixelForgeErrorCode::SUCCESS);
    const auto& webData = dst.getData();
    for (size_t i = 0; i < webData.size(); i += dst.getChannels()) {
        assert(webData[i + 0] % 51 == 0);
        assert(webData[i + 1] % 51 == 0);
        assert(webData[i + 2] % 51 == 0);
    }
    std::cout << "test_floyd_steinberg_dither passed." << std::endl;
}

void test_halftone() {
    std::cout << "Running test_halftone..." << std::endl;
    Image src = create_gradient_image(128, 128);
    Image dst;

    // Dot Halftone
    PixelForgeErrorCode err = AdvancedEffects::Halftone(src, dst, HalftonePattern::DOT, 8, 45.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 128);
    assert(dst.getHeight() == 128);

    // Line Halftone
    err = AdvancedEffects::Halftone(src, dst, HalftonePattern::LINE, 6, 0.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Cross Halftone
    err = AdvancedEffects::Halftone(src, dst, HalftonePattern::CROSS, 10, 30.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);
    std::cout << "test_halftone passed." << std::endl;
}

void test_chromatic_aberration() {
    std::cout << "Running test_chromatic_aberration..." << std::endl;
    Image src = create_gradient_image(64, 64);
    Image dst;

    PixelForgeErrorCode err = AdvancedEffects::ChromaticAberration(src, dst, 3, 2);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 64);
    assert(dst.getHeight() == 64);

    // Check that channels are shifted correctly
    // Pixel (10, 10) in destination should have:
    // Red from (13, 12)
    // Green from (10, 10)
    // Blue from (7, 8)
    uint8_t dstPixel[3];
    dst.getPixel(10, 10, dstPixel, 3);

    uint8_t srcRed[3], srcGreen[3], srcBlue[3];
    src.getPixel(13, 12, srcRed, 3);
    src.getPixel(10, 10, srcGreen, 3);
    src.getPixel(7, 8, srcBlue, 3);

    assert(dstPixel[0] == srcRed[0]);
    assert(dstPixel[1] == srcGreen[1]);
    assert(dstPixel[2] == srcBlue[2]);

    std::cout << "test_chromatic_aberration passed." << std::endl;
}

void test_drop_shadow() {
    std::cout << "Running test_drop_shadow..." << std::endl;
    Image src = create_rgba_test_image(32, 32);
    Image dst;

    ShadowParams params;
    params.offsetX = 4;
    params.offsetY = -3;
    params.blurRadius = 2.0f;
    params.opacity = 0.6f;
    params.shadowColor[0] = 50;
    params.shadowColor[1] = 50;
    params.shadowColor[2] = 50;

    PixelForgeErrorCode err = AdvancedEffects::DropShadow(src, dst, params);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Check size increases: srcW + abs(offsetX) + ceil(blurRadius * 3) * 2
    // 32 + 4 + 6 * 2 = 48 width
    // 32 + 3 + 6 * 2 = 47 height
    assert(dst.getWidth() == 48);
    assert(dst.getHeight() == 47);
    assert(dst.getFormat() == PixelFormat::RGBA8888);

    std::cout << "test_drop_shadow passed." << std::endl;
}

void test_solarize() {
    std::cout << "Running test_solarize..." << std::endl;
    Image src = create_gradient_image(32, 32);
    Image dst;

    PixelForgeErrorCode err = AdvancedEffects::Solarize(src, dst, 128);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Check logic: if val > 128, it becomes 255 - val. Else it remains val.
    const auto& srcData = src.getData();
    const auto& dstData = dst.getData();
    for (size_t i = 0; i < srcData.size(); ++i) {
        if (src.getChannels() == 4 && (i % 4 == 3)) continue;
        uint8_t expected = srcData[i] > 128 ? (255 - srcData[i]) : srcData[i];
        assert(dstData[i] == expected);
    }
    std::cout << "test_solarize passed." << std::endl;
}

void test_posterize() {
    std::cout << "Running test_posterize..." << std::endl;
    Image src = create_gradient_image(32, 32);
    Image dst;

    PixelForgeErrorCode err = AdvancedEffects::Posterize(src, dst, 4);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Verify there are at most 4 unique values per channel
    const auto& dstData = dst.getData();
    std::set<uint8_t> uniqueValues;
    for (size_t i = 0; i < dstData.size(); ++i) {
        if (src.getChannels() == 4 && (i % 4 == 3)) continue;
        uniqueValues.insert(dstData[i]);
    }
    // With 4 levels, levels are: 0, 85, 170, 255
    assert(uniqueValues.size() <= 4);
    for (uint8_t val : uniqueValues) {
        assert(val == 0 || val == 85 || val == 170 || val == 255);
    }
    std::cout << "test_posterize passed." << std::endl;
}

void test_thermal_vision() {
    std::cout << "Running test_thermal_vision..." << std::endl;
    Image src = create_gradient_image(32, 32);
    Image dst;

    PixelForgeErrorCode err = AdvancedEffects::ThermalVision(src, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32);
    assert(dst.getHeight() == 32);
    std::cout << "test_thermal_vision passed." << std::endl;
}

void test_oil_painting() {
    std::cout << "Running test_oil_painting..." << std::endl;
    Image src = create_gradient_image(32, 32);
    Image dst;

    PixelForgeErrorCode err = AdvancedEffects::OilPainting(src, dst, 2, 8);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32);
    assert(dst.getHeight() == 32);
    std::cout << "test_oil_painting passed." << std::endl;
}

void test_pixelate() {
    std::cout << "Running test_pixelate..." << std::endl;
    Image src = create_gradient_image(32, 32);
    Image dst;

    PixelForgeErrorCode err = AdvancedEffects::Pixelate(src, dst, 8);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32);
    assert(dst.getHeight() == 32);

    // Verify 8x8 blocks contain identical color values
    const auto& dstData = dst.getData();
    uint32_t ch = dst.getChannels();
    for (uint32_t by = 0; by < 32; by += 8) {
        for (uint32_t bx = 0; bx < 32; bx += 8) {
            size_t baseIdx = (by * 32 + bx) * ch;
            uint8_t baseR = dstData[baseIdx + 0];
            uint8_t baseG = dstData[baseIdx + 1];
            uint8_t baseB = dstData[baseIdx + 2];
            for (uint32_t dy = 0; dy < 8; ++dy) {
                for (uint32_t dx = 0; dx < 8; ++dx) {
                    size_t idx = ((by + dy) * 32 + (bx + dx)) * ch;
                    assert(dstData[idx + 0] == baseR);
                    assert(dstData[idx + 1] == baseG);
                    assert(dstData[idx + 2] == baseB);
                }
            }
        }
    }
    std::cout << "test_pixelate passed." << std::endl;
}

void run_effects_benchmarks() {
    std::cout << "\n==============================================" << std::endl;
    std::cout << "       RUNNING ADVANCED EFFECTS BENCHMARKS    " << std::endl;
    std::cout << "==============================================" << std::endl;

    Image benchImg = create_gradient_image(512, 512);
    Image out;

    auto start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::FloydSteinbergDither(benchImg, out, DitherPalette::EGA);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms = end - start;
    std::cout << "Floyd-Steinberg Dither (512x512): " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::Halftone(benchImg, out, HalftonePattern::DOT, 4, 45.0f);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Halftone Dot Pattern (512x512):   " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::ChromaticAberration(benchImg, out, 5, 5);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Chromatic Aberration Shift (512x512): " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    ShadowParams params;
    params.offsetX = 10;
    params.offsetY = 10;
    params.blurRadius = 5.0f;
    AdvancedEffects::DropShadow(benchImg, out, params);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Drop Shadow rendering (512x512):  " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::Solarize(benchImg, out, 120);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Solarization (512x512):           " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::Posterize(benchImg, out, 8);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Posterization (512x512):          " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::ThermalVision(benchImg, out);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Thermal Vision simulation:        " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::OilPainting(benchImg, out, 3, 16);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Oil Painting Rank filter:         " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::Pixelate(benchImg, out, 16);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::Pixelate(benchImg, out, 16);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Pixelate filter (512x512):        " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::Glitch(benchImg, out, 8, 10, 54321);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Glitch filter (512x512):          " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    Image left(512, 512, PixelFormat::RGB888);
    Image right(512, 512, PixelFormat::RGB888);
    AdvancedEffects::Anaglyph3D(left, right, out);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Anaglyph3D filter (512x512):      " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::ASCIIArt(benchImg, out, 8, 8);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "ASCII Art render (512x512):       " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::Kaleidoscope(benchImg, out, 8);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Kaleidoscope filter (512x512):    " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    float testMatrix[20] = {
        0.393f, 0.769f, 0.189f, 0.0f, 0.0f,
        0.349f, 0.686f, 0.168f, 0.0f, 0.0f,
        0.272f, 0.534f, 0.131f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f
    };
    AdvancedEffects::ApplyColorMatrix(benchImg, out, testMatrix);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Color Matrix transform (512x512): " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::GenerateMandelbrot(out, 512, 512, -2.0, 0.5, -1.25, 1.25, 100);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Mandelbrot Generation (512x512):  " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::GenerateJulia(out, 512, 512, -0.7, 0.27015, -1.5, 1.5, -1.5, 1.5, 100);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Julia Set Generation (512x512):   " << ms.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    AdvancedEffects::ColorHalftone(benchImg, out, 6, 15.0f, 75.0f, 0.0f, 45.0f);
    end = std::chrono::high_resolution_clock::now();
    ms = end - start;
    std::cout << "Color Halftoning (512x512):       " << ms.count() << " ms" << std::endl;
    
    std::cout << "==============================================\n" << std::endl;
}

void test_glitch() {
    std::cout << "Running test_glitch..." << std::endl;
    Image src = create_gradient_image(64, 64);
    Image dst;
    PixelForgeErrorCode err = AdvancedEffects::Glitch(src, dst, 8, 5, 12345);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 64);
    assert(dst.getHeight() == 64);
    std::cout << "test_glitch passed." << std::endl;
}

void test_anaglyph3d() {
    std::cout << "Running test_anaglyph3d..." << std::endl;
    Image left = create_gradient_image(64, 64);
    Image right = create_gradient_image(64, 64);
    Image dst;
    PixelForgeErrorCode err = AdvancedEffects::Anaglyph3D(left, right, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 64);
    assert(dst.getHeight() == 64);
    
    // Check pixel mixing logic
    uint8_t leftPix[3], rightPix[3], dstPix[3];
    left.getPixel(10, 10, leftPix, 3);
    right.getPixel(10, 10, rightPix, 3);
    dst.getPixel(10, 10, dstPix, 3);
    
    assert(dstPix[0] == leftPix[0]);   // Red from left
    assert(dstPix[1] == rightPix[1]);  // Green from right
    assert(dstPix[2] == rightPix[2]);  // Blue from right
    std::cout << "test_anaglyph3d passed." << std::endl;
}

void test_ascii_art() {
    std::cout << "Running test_ascii_art..." << std::endl;
    Image src = create_gradient_image(64, 64);
    Image dst;
    PixelForgeErrorCode err = AdvancedEffects::ASCIIArt(src, dst, 8, 8);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 64);
    assert(dst.getHeight() == 64);
    std::cout << "test_ascii_art passed." << std::endl;
}

void test_kaleidoscope() {
    std::cout << "Running test_kaleidoscope..." << std::endl;
    Image src = create_gradient_image(64, 64);
    Image dst;
    PixelForgeErrorCode err = AdvancedEffects::Kaleidoscope(src, dst, 6);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 64);
    assert(dst.getHeight() == 64);
    std::cout << "test_kaleidoscope passed." << std::endl;
}

void test_color_matrix() {
    std::cout << "Running test_color_matrix..." << std::endl;
    Image src = create_gradient_image(32, 32);
    Image dst;
    // Simple identity matrix
    float matrix[20] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f
    };
    PixelForgeErrorCode err = AdvancedEffects::ApplyColorMatrix(src, dst, matrix);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32);
    assert(dst.getHeight() == 32);
    
    // Output must match source
    const auto& srcData = src.getData();
    const auto& dstData = dst.getData();
    assert(srcData == dstData);
    std::cout << "test_color_matrix passed." << std::endl;
}

void test_fractals() {
    std::cout << "Running test_fractals..." << std::endl;
    Image mandel, julia;
    PixelForgeErrorCode err = AdvancedEffects::GenerateMandelbrot(mandel, 64, 64, -2.0, 0.5, -1.25, 1.25, 50);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(mandel.getWidth() == 64);
    assert(mandel.getHeight() == 64);

    err = AdvancedEffects::GenerateJulia(julia, 64, 64, -0.7, 0.27015, -1.5, 1.5, -1.5, 1.5, 50);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(julia.getWidth() == 64);
    assert(julia.getHeight() == 64);
    std::cout << "test_fractals passed." << std::endl;
}

void test_color_halftone() {
    std::cout << "Running test_color_halftone..." << std::endl;
    Image src = create_gradient_image(64, 64);
    Image dst;
    PixelForgeErrorCode err = AdvancedEffects::ColorHalftone(src, dst, 4, 15.0f, 75.0f, 0.0f, 45.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 64);
    assert(dst.getHeight() == 64);
    std::cout << "test_color_halftone passed." << std::endl;
}

int main() {
    std::cout << "Starting Advanced Effects Tests..." << std::endl;

    test_floyd_steinberg_dither();
    test_halftone();
    test_chromatic_aberration();
    test_drop_shadow();
    test_solarize();
    test_posterize();
    test_thermal_vision();
    test_oil_painting();
    test_pixelate();
    test_glitch();
    test_anaglyph3d();
    test_ascii_art();
    test_kaleidoscope();
    test_color_matrix();
    test_fractals();
    test_color_halftone();

    std::cout << "All Advanced Effects tests passed successfully!" << std::endl;

    run_effects_benchmarks();

    return 0;
}


