#!/bin/bash -eu

CORE_SOURCES=(
  allocator
  alpha
  animation
  analysis
  bitmap_font
  blend
  bmp_codec
  buffered_reader
  buffered_writer
  byte_stream
  color_convert
  color_space
  config_parser
  convolution
  data_view
  drawing
  effects
  effects_advanced
  enhancement
  expression
  expression_eval
  filter
  gamma
  gif_codec
  histogram
  image
  image_processor_extra
  imgtool_cli
  layer
  logger
  manifest
  math_utils
  memory_stream
  metadata
  processing_graph
  ppm_codec
  stream
  string_utils
  tga_codec
  thread_pool
  tile
  tile_cache
  transform
)

ALL_OBJS=""
for unit in "${CORE_SOURCES[@]}"; do
  $CXX $CXXFLAGS -I$SRC/src -c "$SRC/src/${unit}.cc" -o "${unit}.o"
  ALL_OBJS="${ALL_OBJS} ${unit}.o"
done

# Link all harnesses
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_bmp.cc $ALL_OBJS -o $OUT/fuzz_bmp
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_tga.cc $ALL_OBJS -o $OUT/fuzz_tga
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_gif.cc $ALL_OBJS -o $OUT/fuzz_gif
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_imgtool.cc $ALL_OBJS -o $OUT/fuzz_imgtool
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_expression.cc $ALL_OBJS -o $OUT/fuzz_expression
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_processing_graph.cc $ALL_OBJS -o $OUT/fuzz_processing_graph

