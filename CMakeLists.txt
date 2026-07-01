cmake_minimum_required(VERSION 3.20)

project(PixelForge VERSION 0.2.0 LANGUAGES CXX)

option(PIXELFORGE_BUILD_TESTS "Build PixelForge tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PIXELFORGE_SOURCES
    src/allocator.cc
    src/alpha.cc
    src/animation.cc
    src/analysis.cc
    src/bitmap_font.cc
    src/blend.cc
    src/bmp_codec.cc
    src/buffered_reader.cc
    src/buffered_writer.cc
    src/byte_stream.cc
    src/color_convert.cc
    src/color_space.cc
    src/config_parser.cc
    src/convolution.cc
    src/data_view.cc
    src/drawing.cc
    src/effects.cc
    src/effects_advanced.cc
    src/enhancement.cc
    src/expression.cc
    src/expression_eval.cc
    src/filter.cc
    src/gamma.cc
    src/gif_codec.cc
    src/histogram.cc
    src/image.cc
    src/image_processor_extra.cc
    src/imgtool_cli.cc
    src/layer.cc
    src/logger.cc
    src/manifest.cc
    src/math_utils.cc
    src/memory_stream.cc
    src/metadata.cc
    src/processing_graph.cc
    src/ppm_codec.cc
    src/stream.cc
    src/string_utils.cc
    src/tga_codec.cc
    src/thread_pool.cc
    src/tile.cc
    src/tile_cache.cc
    src/transform.cc
)

add_library(pixelforge STATIC ${PIXELFORGE_SOURCES})
target_include_directories(pixelforge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(pixelforge PRIVATE /W4 /permissive-)
else()
    target_compile_options(pixelforge PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PIXELFORGE_BUILD_TESTS)
    enable_testing()

    foreach(test_name IN ITEMS
            test_core
            test_algorithms
            test_byte_stream
            test_effects_advanced
            test_extra
            test_subsystems
            test_stress
            test_utilities)
        add_executable(${test_name} tests/${test_name}.cc)
        target_link_libraries(${test_name} PRIVATE pixelforge)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
