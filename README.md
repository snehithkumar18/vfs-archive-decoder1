# PixelForge

PixelForge is a C++17 image-processing library and command layer for small desktop, game-tooling, and batch image workflows. It provides basic image codecs, filtering, metadata handling, color conversion, compositing helpers, tiled processing primitives, processing graphs, manifest parsing, expression evaluation, and fuzz-test entry points.

The project is intended to be useful software first. Fuzzing support is part of the quality gate, not the product’s main purpose.

## Current capabilities

- Image containers and codecs:
  - BMP decode support for common Windows bitmap layouts.
  - TGA decode support for uncompressed, RLE, grayscale, and color-mapped images.
  - GIF decode support for GIF87a/GIF89a image streams.
  - Netpbm PPM/PGM/PBM read/write helpers.
- Processing and analysis:
  - Resize, crop, blur, grayscale, sepia, brightness/contrast/gamma, sharpening, and advanced stylized effects.
  - Histogram, convolution, geometric transform, drawing, Canny/Otsu/Hough-style analysis, and SSIM helpers.
- Production subsystems:
  - Bounds-checked byte-stream and in-memory stream abstractions.
  - Color spaces, transfer functions, RGB/XYZ/Lab/HSL/HSV/YCbCr/CMYK conversion helpers.
  - Porter-Duff-style region blending and layer-tree utilities.
  - Tiled image decomposition and tile cache primitives.
  - Non-destructive processing graph validation/execution over core image operations.
  - JSON-like batch manifest parser and INI configuration parser.
  - Expression parser/evaluator with deterministic math built-ins for batch expressions.
  - Priority thread pool, logging, base64/hex utilities, metadata serialization.
- Quality infrastructure:
  - CMake build with unit and integration tests.
  - MSVC local build script.
  - ClusterFuzzLite harness build script.
  - Fuzz harnesses for BMP, TGA, GIF, imgtool command inputs, expressions, and processing graphs.
  - Deterministic seed corpus under `fuzz/corpus`.

## Repository layout

- `src/` — library headers and C++17 implementation files.
- `tests/` — deterministic unit, integration, utility, stress, and subsystem tests.
- `fuzz/` — libFuzzer-compatible harnesses and seed corpora.
- `.clusterfuzzlite/` — fuzz build script for ClusterFuzzLite-style environments.
- `tools/` — local helper scripts for corpus generation and MSVC build verification.
- `docs/` — architecture, roadmap, and submission-readiness notes.

## Build and test

### Windows / MSVC

From a normal PowerShell prompt in the repository root:

```powershell
tools\build_msvc.bat
```

This script locates Visual Studio Build Tools, runs two fast smoke tests, configures CMake with the MSVC NMake generator, builds the full library, and runs the CTest suite.

### CMake

If your compiler environment is already initialized:

```bash
cmake -S . -B build/cmake -DPIXELFORGE_BUILD_TESTS=ON
cmake --build build/cmake
ctest --test-dir build/cmake --output-on-failure
```

The build is offline and does not require downloading dependencies.

## Fuzzing

Harnesses live in `fuzz/`:

- `fuzz_bmp.cc`
- `fuzz_tga.cc`
- `fuzz_gif.cc`
- `fuzz_imgtool.cc`
- `fuzz_expression.cc`
- `fuzz_processing_graph.cc`

Seed corpus files live under:

- `fuzz/corpus/fuzz_bmp/`
- `fuzz/corpus/fuzz_tga/`
- `fuzz/corpus/fuzz_gif/`
- `fuzz/corpus/fuzz_imgtool/`
- `fuzz/corpus/fuzz_expression/`
- `fuzz/corpus/fuzz_processing_graph/`

Regenerate deterministic seeds with:

```bash
python tools/generate_seed_corpus.py
```

## Security and PoC policy

PixelForge keeps fuzz harnesses and sanitizers enabled for vulnerability discovery and regression testing. Proof-of-concept crash inputs should be raw harness inputs and must be validated against the exact unpatched revision they target.

This repository should not include credentials, network-dependent setup, generated filler code, duplicate padding, or artificial line-count inflation.
