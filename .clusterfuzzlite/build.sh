#!/bin/bash -eu
# Compile all core source files
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/image.cc -o image.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/allocator.cc -o allocator.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/bmp_codec.cc -o bmp_codec.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/tga_codec.cc -o tga_codec.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/gif_codec.cc -o gif_codec.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/ppm_codec.cc -o ppm_codec.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/math_utils.cc -o math_utils.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/convolution.cc -o convolution.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/transform.cc -o transform.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/histogram.cc -o histogram.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/drawing.cc -o drawing.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/analysis.cc -o analysis.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/effects.cc -o effects.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/effects_advanced.cc -o effects_advanced.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/enhancement.cc -o enhancement.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/filter.cc -o filter.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/metadata.cc -o metadata.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/imgtool_cli.cc -o imgtool_cli.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/string_utils.cc -o string_utils.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/config_parser.cc -o config_parser.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/thread_pool.cc -o thread_pool.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/logger.cc -o logger.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/image_processor_extra.cc -o image_processor_extra.o

# Object collection
ALL_OBJS="image.o allocator.o bmp_codec.o tga_codec.o gif_codec.o ppm_codec.o math_utils.o convolution.o transform.o histogram.o drawing.o analysis.o effects.o effects_advanced.o enhancement.o filter.o metadata.o imgtool_cli.o string_utils.o config_parser.o thread_pool.o logger.o image_processor_extra.o"

# Link all harnesses
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_bmp.cc $ALL_OBJS -o $OUT/fuzz_bmp
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_tga.cc $ALL_OBJS -o $OUT/fuzz_tga
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_gif.cc $ALL_OBJS -o $OUT/fuzz_gif
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_imgtool.cc $ALL_OBJS -o $OUT/fuzz_imgtool

