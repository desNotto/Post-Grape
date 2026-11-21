@echo off
rem Post-Grape - build script (Windows, MSVC 2022 + CMake + Ninja).
rem Result: bin\Post-Grape.vst3
setlocal
cd /d "%~dp0"

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul

if not exist build\build.ninja (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
)
cmake --build build --config Release || exit /b 1
build\PostGrapeTests.exe || exit /b 1
echo.
echo Done: bin\Post-Grape.vst3
endlocal
