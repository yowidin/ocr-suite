# OCR Suite

A tool for recognizing text in a video file and storing it in a SQLite3 database.

## Installation
- Grab a binary from the releases page, or build it yourself.
- Download the [Tesseract OCR data](https://github.com/tesseract-ocr/tessdata/releases/)

## Building

### Requirements
- [CMake](https://cmake.org/download/)
- A modern enough C++ compiler (with C++17 support)
- [conan](https://docs.conan.io/en/latest/installation.html)

### Steps

1. Clone the repository

2. Make a build directory and cd into it
```shell
mkdir -p build/release && cd build/release
```

3. Install the dependencies
```shell
conan install ../../ --build=missing -s build_type=Release
```

4. Build the project
```shell
cmake --preset release ../..
cmake --build .
```

You will find the `orc-suite` binary in the build directory.
