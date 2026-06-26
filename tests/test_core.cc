#include "../src/errors.h"
#include "../src/logger.h"
#include "../src/image.h"
#include "../src/allocator.h"
#include "../src/bmp_codec.h"
#include "../src/tga_codec.h"
#include "../src/gif_codec.h"
#include "../src/ppm_codec.h"
#include "../src/filter.h"
#include "../src/math_utils.h"
#include "../src/convolution.h"
#include "../src/transform.h"
#include "../src/histogram.h"
#include "../src/drawing.h"
#include "../src/analysis.h"
#include "../src/effects.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <cstring>
#include <memory>
#include <algorithm>

using namespace PixelForge;

// Helper to create a simple synthetic checkerboard image
Image create_checkerboard(uint32_t w, uint32_t h, uint32_t block_size = 8) {
    Image img(w, h, PixelFormat::RGB888);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            bool is_white = ((x / block_size) + (y / block_size)) % 2 == 0;
            uint8_t color[3] = { static_cast<uint8_t>(is_white ? 255 : 0),
                                 static_cast<uint8_t>(is_white ? 255 : 0),
                                 static_cast<uint8_t>(is_white ? 255 : 0) };
            img.setPixel(x, y, color, 3);
        }
    }
    return img;
}

void test_logger() {
    Logger& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::LOG_DEBUG);
    logger.debug("Debug log test.");
    logger.info("Info log test.");
    logger.warn("Warn log test.");
    logger.error("Error log test.");
    assert(logger.getLogLevel() == LogLevel::LOG_DEBUG);
}

void test_image() {
    Image img(100, 100, PixelFormat::RGBA8888);
    assert(img.getWidth() == 100);
    assert(img.getHeight() == 100);
    assert(img.getFormat() == PixelFormat::RGBA8888);
    assert(img.getChannels() == 4);
    assert(img.getData().size() == 40000);

    uint8_t pixel[4] = {255, 128, 64, 32};
    PixelForgeErrorCode err = img.setPixel(10, 20, pixel, 4);
    assert(err == PixelForgeErrorCode::SUCCESS);

    uint8_t outPixel[4] = {0};
    err = img.getPixel(10, 20, outPixel, 4);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(outPixel[0] == 255);
    assert(outPixel[1] == 128);
    assert(outPixel[2] == 64);
    assert(outPixel[3] == 32);

    err = img.convertTo(PixelFormat::Grayscale);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(img.getChannels() == 1);
    assert(img.getFormat() == PixelFormat::Grayscale);
}

void test_allocator() {
    auto allocator = FrameBufferAllocator::create(2);
    
    // Acquire a buffer
    auto img1 = allocator->acquire(64, 64, PixelFormat::RGB888);
    assert(img1 != nullptr);
    assert(allocator->getActiveCount() == 1);
    assert(allocator->getFreeCount() == 0);

    // Acquire another
    auto img2 = allocator->acquire(128, 128, PixelFormat::RGBA8888);
    assert(img2 != nullptr);
    assert(allocator->getActiveCount() == 2);
    assert(allocator->getFreeCount() == 0);

    // Release img1
    size_t capacityBefore = img1->getData().capacity();
    img1.reset();
    assert(allocator->getActiveCount() == 1);
    assert(allocator->getFreeCount() == 1);

    // Re-acquire
    auto img3 = allocator->acquire(64, 64, PixelFormat::RGB888);
    assert(img3 != nullptr);
    assert(allocator->getActiveCount() == 2);
    assert(allocator->getFreeCount() == 0);
    assert(img3->getData().capacity() >= capacityBefore);
}

void test_bmp_codec() {
    Image img = create_checkerboard(32, 32, 4);
    std::vector<uint8_t> encoded_data;
    
    // Encode RGB BMP
    PixelForgeErrorCode err = BMPCodec::Encode(img, encoded_data);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(!encoded_data.empty());

    // Decode RGB BMP
    Image decoded_img;
    err = BMPCodec::Decode(encoded_data, decoded_img);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(decoded_img.getWidth() == 32);
    assert(decoded_img.getHeight() == 32);
    assert(decoded_img.getChannels() == 3);

    // Check pixel match
    uint8_t p1[3] = {0}, p2[3] = {0};
    img.getPixel(0, 0, p1, 3);
    decoded_img.getPixel(0, 0, p2, 3);
    assert(p1[0] == p2[0] && p1[1] == p2[1] && p1[2] == p2[2]);
}

void test_tga_codec() {
    Image img = create_checkerboard(32, 32, 4);
    std::vector<uint8_t> encoded_uncompressed;
    std::vector<uint8_t> encoded_rle;

    // Encode Uncompressed TGA
    PixelForgeErrorCode err = TGACodec::Encode(img, encoded_uncompressed, false);
    assert(err == PixelForgeErrorCode::SUCCESS);
    
    // Encode RLE Compressed TGA
    err = TGACodec::Encode(img, encoded_rle, true);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Decode Uncompressed
    Image dec1;
    err = TGACodec::Decode(encoded_uncompressed, dec1);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dec1.getWidth() == 32);
    assert(dec1.getHeight() == 32);

    // Decode RLE
    Image dec2;
    err = TGACodec::Decode(encoded_rle, dec2);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dec2.getWidth() == 32);
    assert(dec2.getHeight() == 32);

    // Validate colors match
    uint8_t p1[3] = {0}, p2[3] = {0};
    dec1.getPixel(4, 4, p1, 3);
    dec2.getPixel(4, 4, p2, 3);
    assert(p1[0] == p2[0]);
}

void test_ppm_codec() {
    Image img = create_checkerboard(32, 32, 4);
    std::vector<uint8_t> encoded_p6;
    std::vector<uint8_t> encoded_p5;

    // P6 Encode
    PixelForgeErrorCode err = PPMCodec::EncodeP6(img, encoded_p6);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // P6 Decode
    Image dec_p6;
    err = PPMCodec::Decode(encoded_p6, dec_p6);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dec_p6.getWidth() == 32);
    assert(dec_p6.getFormat() == PixelFormat::RGB888);

    // P5 Encode (Grayscale)
    err = PPMCodec::EncodeP5(img, encoded_p5);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // P5 Decode
    Image dec_p5;
    err = PPMCodec::Decode(encoded_p5, dec_p5);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dec_p5.getWidth() == 32);
    assert(dec_p5.getFormat() == PixelFormat::Grayscale);
}

void test_filters() {
    Image img = create_checkerboard(32, 32, 8);

    // Bilinear Resize
    Image* resized = apply_resize(&img, 64, 64, nullptr, "");
    assert(resized != nullptr);
    assert(resized->getWidth() == 64);
    assert(resized->getHeight() == 64);
    delete resized;

    // Box Blur
    Image* blurred = apply_blur(&img, 2, nullptr, "");
    assert(blurred != nullptr);
    assert(blurred->getWidth() == 32);
    delete blurred;

    // Crop
    Image* cropped = apply_crop(&img, 4, 4, 16, 16, nullptr, "");
    assert(cropped != nullptr);
    assert(cropped->getWidth() == 16);
    assert(cropped->getHeight() == 16);
    delete cropped;

    // Grayscale
    Image* gray = apply_grayscale(&img, nullptr, "");
    assert(gray != nullptr);
    assert(gray->getChannels() == 1);
    delete gray;
}

void test_math_utils() {
    // Test RGB <-> HSV
    uint8_t r = 255, g = 128, b = 64;
    HSVColor hsv = RGBToHSV(r, g, b);
    uint8_t r2, g2, b2;
    HSVToRGB(hsv, r2, g2, b2);
    assert(std::abs(r - r2) <= 1);
    assert(std::abs(g - g2) <= 1);
    assert(std::abs(b - b2) <= 1);

    // Test RGB <-> HSL
    HSLColor hsl = RGBToHSL(r, g, b);
    HSLToRGB(hsl, r2, g2, b2);
    assert(std::abs(r - r2) <= 1);
    assert(std::abs(g - g2) <= 1);
    assert(std::abs(b - b2) <= 1);

    // Test RGB <-> YCbCr
    YCbCrColor ycc = RGBToYCbCr(r, g, b);
    YCbCrToRGB(ycc, r2, g2, b2);
    assert(std::abs(r - r2) <= 2);
    assert(std::abs(g - g2) <= 2);
    assert(std::abs(b - b2) <= 2);

    // Matrix
    Matrix3x3 mat(1, 0, 0, 0, 1, 0, 0, 0, 1);
    Vector3 vec(10, 20, 30);
    Vector3 res = mat.multiply(vec);
    assert(res.x == 10 && res.y == 20 && res.z == 30);
}

void test_convolution() {
    Image img = create_checkerboard(32, 32, 4);
    Image dst;

    // Gaussian Blur
    PixelForgeErrorCode err = Convolution::Apply(img, dst, Convolution::GetGaussianBlur5x5());
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32);

    // Sobel X
    err = Convolution::Apply(img, dst, Convolution::GetSobelX3x3());
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Emboss
    err = Convolution::Apply(img, dst, Convolution::GetEmboss3x3());
    assert(err == PixelForgeErrorCode::SUCCESS);
}

void test_transform() {
    Image img = create_checkerboard(32, 32, 8);
    Image dst;

    // Flip H
    PixelForgeErrorCode err = Transform::FlipHorizontal(img, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);
    
    // Rotate 90
    err = Transform::Rotate90(img, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() == 32 && dst.getHeight() == 32);

    // Rotate Arbitrary
    err = Transform::Rotate(img, dst, 45.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(dst.getWidth() > 32);

    // Translate
    err = Transform::Translate(img, dst, 5, -5);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

void test_histogram() {
    Image img = create_checkerboard(32, 32, 8);
    ImageHistogram hist;
    
    PixelForgeErrorCode err = Histogram::Compute(img, hist);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(hist.gray.size() == 256);

    Image dst;
    err = Histogram::Equalize(img, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);

    err = Histogram::Stretch(img, dst, 10, 240);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

void test_drawing() {
    Image img(64, 64, PixelFormat::RGBA8888);
    ColorRGBA red(255, 0, 0, 255);
    ColorRGBA green(0, 255, 0, 128); // translucent

    // Pixel
    Drawing::DrawPixel(img, 10, 10, red);

    // Line
    PixelForgeErrorCode err = Drawing::DrawLine(img, 0, 0, 63, 63, red);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Rect
    err = Drawing::DrawRect(img, 5, 5, 20, 20, green, true);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Circle
    err = Drawing::DrawCircle(img, 32, 32, 10, red, false);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Text
    err = Drawing::DrawText(img, 2, 2, "PixelForge", red);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

void test_analysis() {
    Image img = create_checkerboard(32, 32, 8);
    
    // Stats
    ImageStats stats;
    PixelForgeErrorCode err = Analysis::ComputeStats(img, stats);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(stats.entropy > 0);

    // Otsu
    uint8_t threshold = Analysis::ComputeOtsuThreshold(img);
    assert(threshold > 0);

    // Connected Components
    Image labeled;
    uint32_t num_labels = 0;
    err = Analysis::LabelComponents(img, labeled, num_labels);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(num_labels > 0);

    // Canny Edges
    Image edges;
    err = Analysis::CannyEdges(img, edges, 50, 150);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Hough Lines
    std::vector<HoughLine> lines;
    err = Analysis::HoughLines(edges, lines, 5);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // SSIM
    float ssim = 0.0f;
    err = Analysis::ComputeSSIM(img, img, ssim);
    assert(err == PixelForgeErrorCode::SUCCESS);
    assert(std::abs(ssim - 1.0f) < 0.001f);
}

void test_effects() {
    Image img = create_checkerboard(32, 32, 8);
    Image dst;

    // Brightness/Contrast
    PixelForgeErrorCode err = Effects::AdjustBrightnessContrast(img, dst, 30.0f, 15.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Gamma
    err = Effects::GammaCorrection(img, dst, 1.8f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Sepia
    err = Effects::Sepia(img, dst);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Vignette
    err = Effects::Vignette(img, dst, 0.8f, 2.0f);
    assert(err == PixelForgeErrorCode::SUCCESS);

    // Color Blindness
    err = Effects::SimulateColorBlindness(img, dst, ColorBlindType::PROTANOPIA);
    assert(err == PixelForgeErrorCode::SUCCESS);
}

int main() {
    std::cout << "Running PixelForge Core Tests..." << std::endl;
    
    test_logger();
    std::cout << "-> Logger tests passed." << std::endl;

    test_image();
    std::cout << "-> Image tests passed." << std::endl;

    test_allocator();
    std::cout << "-> Allocator tests passed." << std::endl;

    test_bmp_codec();
    std::cout << "-> BMP Codec tests passed." << std::endl;

    test_tga_codec();
    std::cout << "-> TGA Codec tests passed." << std::endl;

    test_ppm_codec();
    std::cout << "-> Netpbm PPM Codec tests passed." << std::endl;

    test_filters();
    std::cout << "-> Filters tests passed." << std::endl;

    test_math_utils();
    std::cout << "-> Math & Color tests passed." << std::endl;

    test_convolution();
    std::cout << "-> Convolution kernel tests passed." << std::endl;

    test_transform();
    std::cout << "-> Spatial transform tests passed." << std::endl;

    test_histogram();
    std::cout << "-> Histogram tests passed." << std::endl;

    test_drawing();
    std::cout << "-> Drawing utilities tests passed." << std::endl;

    test_analysis();
    std::cout << "-> Image analysis & CV tests passed." << std::endl;

    test_effects();
    std::cout << "-> Image effects & correction tests passed." << std::endl;

    std::cout << "\nAll PixelForge Core Tests Passed Successfully!" << std::endl;
    return 0;
}
