cmake_minimum_required(VERSION 3.6)
project(ocr_suite VERSION 0.0.1 LANGUAGES CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(NO_OUTPUT_DIRS)

# find_package(ffmpeg REQUIRED CONFIG)
# find_package(tesseract REQUIRED CONFIG)
# find_package(sqlite3 REQUIRED CONFIG)
# find_package(lyra CONFIG REQUIRED)
# find_package(spdlog CONFIG REQUIRED)
# find_package(indicators CONFIG REQUIRED)
# find_package(Boost CONFIG REQUIRED COMPONENTS filesystem)


add_executable(ocr_suite
    src/main.cpp
    src/video.cpp
    src/bmp.cpp
    src/ocr.cpp
    src/database.cpp
    src/db/database.cpp
    src/db/statement.cpp
    src/speed_meter.cpp
    src/options.cpp)

target_link_libraries(ocr_suite PRIVATE ${CONAN_LIBS}
    # ffmpeg::avcodec ffmpeg::avformat ffmpeg::avutil ffmpeg::swscale
    # tesseract::libtesseract
    # SQLite::SQLite3 bfg::lyra spdlog::spdlog Boost::filesystem indicators::indicators
    )

target_include_directories(ocr_suite
    PUBLIC include
    )

set_target_properties(ocr_suite PROPERTIES
    OUTPUT_NAME ocr-suite

    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    )

if (BUILD_TESTS)
  add_subdirectory(test)
endif ()

option(DOWNLOAD_TESS_DATA "Download tesseract data" OFF)
if (DOWNLOAD_TESS_DATA)
  include(FetchContent)
  set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
  FetchContent_Declare(
      tessdata
      URL https://github.com/tesseract-ocr/tessdata/archive/refs/tags/4.1.0.zip
      URL_HASH SHA1=b75493c8567d7c4b7c9e2d771cca0d3443983196
  )
  if (NOT tessdata_POPULATED)
    message(STATUS "Downloading Tesseract data")
    FetchContent_Populate(tessdata)

    message(STATUS "Copying Tesseract data")
    file(COPY ${tessdata_SOURCE_DIR}/ DESTINATION ${CMAKE_BINARY_DIR}/tessdata)
  endif ()
endif ()

include(cmake/install_helper.cmake)
include(cmake/package_helper.cmake)
