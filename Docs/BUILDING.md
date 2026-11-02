# Building

## Requirements
- CMake ≥ 3.20
- C++20 compiler (GCC 11+, Clang 13+, MSVC 2022)
- Optional: JUCE 7 or 8 (developer-supplied path)

## DSP + tests only
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCOSMIC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## With JUCE plugin
```bash
cmake -S . -B build -DJUCE_PATH=/absolute/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

No network access or FetchContent is used.
