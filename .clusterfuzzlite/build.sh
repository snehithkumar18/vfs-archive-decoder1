#!/bin/bash -eu
# Compile core VFS source
$CXX $CXXFLAGS -c -I$SRC/src $SRC/src/vfs.cc -o vfs.o

# Link all three harnesses
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_mount.cc vfs.o -o $OUT/fuzz_mount
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_extract.cc vfs.o -o $OUT/fuzz_extract
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE $SRC/fuzz/fuzz_vfs.cc vfs.o -o $OUT/fuzz_vfs
