#!/bin/sh
# Post-Grape - build script (macOS: Xcode command line tools + CMake).
# Result: bin/Post-Grape/Post-Grape.vst3 and bin/Post-Grape/Post-Grape.component
set -e
cd "$(dirname "$0")"
if [ ! -d external/JUCE ]; then
    git clone --depth 1 --branch 8.0.17-security https://github.com/juce-framework/JUCE.git external/JUCE
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --parallel 4
./build/PostGrapeTests
echo
echo "Done: bin/Post-Grape/"
