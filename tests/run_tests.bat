@echo off
echo Compiling PixelForge Core Tests...
g++ -std=c++17 -Wall -Wextra src/image.cc src/allocator.cc src/logger.cc src/bmp_codec.cc src/tga_codec.cc src/gif_codec.cc src/ppm_codec.cc src/math_utils.cc src/convolution.cc src/transform.cc src/histogram.cc src/drawing.cc src/analysis.cc src/effects.cc src/effects_advanced.cc src/filter.cc src/metadata.cc src/string_utils.cc src/config_parser.cc src/thread_pool.cc tests/test_core.cc -o tests/test_core.exe
if %ERRORLEVEL% NEQ 0 (
    echo Compilation of core tests failed!
    exit /b %ERRORLEVEL%
)

echo Compiling PixelForge Algorithm Tests...
g++ -std=c++17 -Wall -Wextra src/image.cc src/allocator.cc src/logger.cc src/bmp_codec.cc src/tga_codec.cc src/gif_codec.cc src/ppm_codec.cc src/math_utils.cc src/convolution.cc src/transform.cc src/histogram.cc src/drawing.cc src/analysis.cc src/effects.cc src/effects_advanced.cc src/filter.cc src/metadata.cc src/string_utils.cc src/config_parser.cc src/thread_pool.cc tests/test_algorithms.cc -o tests/test_algorithms.exe
if %ERRORLEVEL% NEQ 0 (
    echo Compilation of algorithm tests failed!
    exit /b %ERRORLEVEL%
)

echo Compiling PixelForge Stress Tests...
g++ -std=c++17 -Wall -Wextra src/image.cc src/allocator.cc src/logger.cc src/bmp_codec.cc src/tga_codec.cc src/gif_codec.cc src/ppm_codec.cc src/math_utils.cc src/convolution.cc src/transform.cc src/histogram.cc src/drawing.cc src/analysis.cc src/effects.cc src/effects_advanced.cc src/filter.cc src/metadata.cc src/string_utils.cc src/config_parser.cc src/thread_pool.cc tests/test_stress.cc -o tests/test_stress.exe
if %ERRORLEVEL% NEQ 0 (
    echo Compilation of stress tests failed!
    exit /b %ERRORLEVEL%
)

echo Compiling PixelForge Advanced Effects Tests...
g++ -std=c++17 -Wall -Wextra src/image.cc src/allocator.cc src/logger.cc src/bmp_codec.cc src/tga_codec.cc src/gif_codec.cc src/ppm_codec.cc src/math_utils.cc src/convolution.cc src/transform.cc src/histogram.cc src/drawing.cc src/analysis.cc src/effects.cc src/effects_advanced.cc src/filter.cc src/metadata.cc src/string_utils.cc src/config_parser.cc src/thread_pool.cc tests/test_effects_advanced.cc -o tests/test_effects_advanced.exe
if %ERRORLEVEL% NEQ 0 (
    echo Compilation of advanced effects tests failed!
    exit /b %ERRORLEVEL%
)

echo Running core tests...
tests\test_core.exe
if %ERRORLEVEL% NEQ 0 (
    echo Core tests failed!
    exit /b %ERRORLEVEL%
)

echo Running algorithm tests...
tests\test_algorithms.exe
if %ERRORLEVEL% NEQ 0 (
    echo Algorithm tests failed!
    exit /b %ERRORLEVEL%
)

echo Running stress tests...
tests\test_stress.exe
if %ERRORLEVEL% NEQ 0 (
    echo Stress tests failed!
    exit /b %ERRORLEVEL%
)

echo Running advanced effects tests...
tests\test_effects_advanced.exe
if %ERRORLEVEL% NEQ 0 (
    echo Advanced effects tests failed!
    exit /b %ERRORLEVEL%
)

echo All tests passed successfully!

