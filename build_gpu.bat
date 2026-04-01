@echo off
setlocal
echo ========================================
echo   GPU Engine Build Script (VS 2022)
echo ========================================

echo [1/5] Cleaning old build files (if possible)...
if exist build_cuda rd /s /q build_cuda 2>nul
if not exist build_cuda mkdir build_cuda

echo [2/5] Initializing MSVC 2022 environment...
:: This sets up the compiler, linker, and include paths for VS 2022
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

if "%VCINSTALLDIR%"=="" (
    echo ERROR: Visual Studio 2022 environment failed to initialize!
    exit /b 1
)

echo [3/5] Configuring CMake with Ninja generator...
:: We explicitly point to the VS 2022 compiler and the NVCC compiler
cmake -G Ninja -S . -B build_cuda ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CUDA_COMPILER="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/bin/nvcc.exe" ^
    -DCMAKE_C_COMPILER="cl.exe" ^
    -DCMAKE_CXX_COMPILER="cl.exe"

if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo [4/5] Compiling and Linking...
cmake --build build_cuda --config Release

if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed!
    exit /b %ERRORLEVEL%
)

echo [5/5] Finalizing deployment...
copy /Y "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.4\bin\cudart64_12.dll" "build_cuda\cudart64_12.dll"

echo.
echo SUCCESS! You can now run the simulation:
echo .\build_cuda\SimulationEngine.exe
echo.
endlocal
