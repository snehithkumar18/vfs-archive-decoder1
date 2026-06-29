@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /std:c++17 /EHsc /W3 src/image.cc src/allocator.cc src/logger.cc src/bmp_codec.cc src/tga_codec.cc src/gif_codec.cc src/ppm_codec.cc src/math_utils.cc src/convolution.cc src/transform.cc src/histogram.cc src/drawing.cc src/analysis.cc src/effects.cc src/effects_advanced.cc src/enhancement.cc src/filter.cc src/metadata.cc src/imgtool_cli.cc src/string_utils.cc src/config_parser.cc src/thread_pool.cc tests/test_core.cc /Isrc /Fetest_core.exe
