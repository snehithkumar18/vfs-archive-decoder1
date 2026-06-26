# PixelForge (Production-Grade C++17 Image Decoding & Processing Library)

`PixelForge` is a robust, production-grade C++17 systems programming library designed for decoding, encoding, filtering, and analyzing standard digital image formats in embedded, graphical, and game development environments.

The library features memory-recycled buffer allocation, comprehensive pixel geometry shaders, and advanced computer vision and image enhancement pipelines.

## Features

* **Standard File Format Codecs**:
  * **BMP (Windows Bitmap)**: Support for v3/v4/v5 headers, deep bit depths (24-bit/32-bit), color palettes, and standard RLE-8 compression.
  * **TGA (Truevision Targa)**: Support for uncompressed and RLE-compressed true-color, grayscale, and color-mapped formats.
  * **GIF (Graphics Interchange Format)**: Decompresses animated multi-frame GIF87a/GIF89a containers using a table-driven LZW bitstream decoder.
  * **PPM (Netpbm Portable Pixmap)**: Support for P1-P6 text and binary formats (grayscale, RGB).

* **Advanced Filter & Shader Engine**:
  * **Traditional effects**: Grayscale, sepia, box blurs, brightness, contrast, gamma, and vignette adjustments.
  * **Color Bindness Simulation**: LMS-based simulations for protanopia, deuteranopia, and tritanopia.
  * **Stylized advanced filters**: rotated grid halftone, Floyd-Steinberg error-diffusion dithering, Gaussian drop shadows with alpha-compositing, chromatic aberration, solarization, posterization, oil painting, kaleidoscope, glitch strips, anaglyph 3D, and character ASCII art.

* **Computer Vision & Image Analysis**:
  * **Edge Detection**: Canny edge detector with hysteresis thresholding and Gaussian pre-smoothing.
  * **Feature Extraction**: Otsu binarization, Connected Component Labeling (CCL), and Hough Transform for detecting lines.
  * **Quality Assessment**: Structural Similarity Index Measure (SSIM) for comparing two images.

* **Performance & Subsystems**:
  * **Recyclable Allocator**: FrameBufferAllocator recycles memory buffers for multi-frame animations to avoid runtime allocation overhead.
  * **Task Thread Pool**: Thread pool with job priority scheduling for parallelizing heavy filter computations.
  * **INI Config Parser**: Parses configurations to automate batch image filtering pipelines.
  * **Hex / Base64 Codecs**: Standard text representations for metadata handling.

## Directory Structure

* `src/`: Core library header and implementation source files.
  * `image.h` / `image.cc`: Core image class and conversion helpers.
  * `allocator.h` / `allocator.cc`: Memory recyclers.
  * `*_codec.h` / `*_codec.cc`: BMP, TGA, GIF, PPM file decoders.
  * `filter.h` / `filter.cc` / `effects_*.h` / `effects_*.cc`: Blur, transform, advanced effects.
  * `analysis.h` / `analysis.cc`: Canny, Otsu, Hough, SSIM.
  * `thread_pool.h` / `thread_pool.cc` / `logger.h` / `logger.cc`: Task scheduling, logging.
* `fuzz/`: Fuzzing harnesses targeting BMP, TGA, GIF, and imgtool CLI.
* `.clusterfuzzlite/`: Build configurations and scripts for ClusterFuzzLite integration.
* `tests/`: Automated unit tests, stress tests, and benchmarks.
* `pocs/`: Generated proof-of-concept files triggering security boundary assertions.

## Building and Running Tests

To compile and run the test suite locally (expects `g++` supporting C++17):
```bash
# Execute local runner batch script (Windows)
tests\run_tests.bat
```
