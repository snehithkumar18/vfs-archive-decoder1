@echo off
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Installer was not found.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (
  echo Visual C++ Build Tools were not found.
  exit /b 1
)

call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

if not exist build mkdir build
pushd build

cl /nologo /std:c++17 /EHsc /W4 /I ..\src ..\tests\test_byte_stream.cc ..\src\byte_stream.cc /Fe:test_byte_stream.exe
if errorlevel 1 exit /b %errorlevel%
test_byte_stream.exe
if errorlevel 1 exit /b %errorlevel%

cl /nologo /std:c++17 /EHsc /W4 /I ..\src ^
  ..\src\image.cc ..\src\allocator.cc ..\src\logger.cc ^
  ..\src\bmp_codec.cc ..\src\tga_codec.cc ..\src\gif_codec.cc ..\src\ppm_codec.cc ^
  ..\src\math_utils.cc ..\src\convolution.cc ..\src\transform.cc ^
  ..\src\histogram.cc ..\src\drawing.cc ..\src\analysis.cc ^
  ..\src\effects.cc ..\src\effects_advanced.cc ..\src\enhancement.cc ^
  ..\src\filter.cc ..\src\metadata.cc ..\src\imgtool_cli.cc ^
  ..\src\string_utils.cc ..\src\config_parser.cc ..\src\thread_pool.cc ^
  ..\tests\test_core.cc /Fe:test_core.exe
if errorlevel 1 exit /b %errorlevel%
test_core.exe
if errorlevel 1 exit /b %errorlevel%

popd

set "CMAKE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CTEST=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
if exist "%CMAKE%" (
  "%CMAKE%" -S . -B build\cmake-nmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
  if errorlevel 1 exit /b %errorlevel%
  "%CMAKE%" --build build\cmake-nmake --parallel 4
  if errorlevel 1 exit /b %errorlevel%
  "%CTEST%" --test-dir build\cmake-nmake --output-on-failure
  if errorlevel 1 exit /b %errorlevel%
)

echo PixelForge MSVC smoke build passed.
exit /b 0
