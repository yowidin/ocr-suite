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
conan install ../.. --build=missing -s build_type=Release
```

4. Build the project
```shell
conan build ../..
```

You will find the `orc-suite` binary in the build directory.

## Automating recognition
You can use the `ocs-watcher` helper script to run the OCR Suite on all video files in a directory. 
It will automatically detect when a new file is added and run the OCR Suite on it, as well as run
OCR Suite periodically on all files in the directory, thus keeping the database up to date for a video file,
currently being recorded.

### Installation
Use `pip` to install the script:
```shell
pip install ./tools/ocs-watcher
```

### Usage
You will need to pass the video files directory as well as the path to the `ocr-suite` binary to the script:
```shell
ocs-watcher -o ./bin/dir/ocr-suite -i ./movies/dir/ -t ./tesserarct/data/dir/ -f 10
```