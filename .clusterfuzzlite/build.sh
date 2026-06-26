#!/bin/bash -eu
# Compile all core source files
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/vfs.cc -o vfs.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/vfs_parser.cc -o vfs_parser.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/vfs_cache.cc -o vfs_cache.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/vfs_stats.cc -o vfs_stats.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/compression_rle.cc -o compression_rle.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/compression_huffman.cc -o compression_huffman.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/compression_lzw.cc -o compression_lzw.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/checksum.cc -o checksum.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/allocator.cc -o allocator.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/path_utils.cc -o path_utils.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/logger.cc -o logger.o
$CXX $CXXFLAGS -I$SRC/src -c $SRC/src/shell.cc -o shell.o

# Object collection
ALL_OBJS="vfs.o vfs_parser.o vfs_cache.o vfs_stats.o compression_rle.o compression_huffman.o compression_lzw.o checksum.o allocator.o path_utils.o logger.o shell.o"

# Link all harnesses
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_mount.cc $ALL_OBJS -o $OUT/fuzz_mount
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_extract.cc $ALL_OBJS -o $OUT/fuzz_extract
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_vfs.cc $ALL_OBJS -o $OUT/fuzz_vfs
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_shell.cc $ALL_OBJS -o $OUT/fuzz_shell
