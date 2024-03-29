cmake_minimum_required(VERSION 3.6)
project(ocr_suite VERSION 0.3.0 LANGUAGES CXX)

#################################################################################
### Setup platform-specific flags
set(OCS_WINDOWS OFF)
set(OCS_APPLE OFF)
set(OCS_UNIX OFF)

if (APPLE)
   set(OCS_APPLE ON)
elseif (UNIX)
   set(OCS_UNIX ON)
elseif (WIN32)
   set(OCS_WINDOWS ON)
endif ()

message(STATUS "Windows: ${OCS_WINDOWS}")
message(STATUS "Apple: ${OCS_APPLE}")
message(STATUS "Unix: ${OCS_UNIX}")

set(OCS_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(OCS_GENERATED_INCLUDE_DIR "${OCS_GENERATED_DIR}/include")
set(OCS_GENERATED_CONFIG_HEADER "${OCS_GENERATED_INCLUDE_DIR}/ocs/config.h")
configure_file(cmake/config.h.in ${OCS_GENERATED_CONFIG_HEADER} @ONLY)

################################################################################
### Dependencies
find_package(ffmpeg CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(indicators CONFIG REQUIRED)

################################################################################
### FFMPEG helper
add_library(ffmpeg_helper STATIC
   src/ffmpeg/decoder.cpp
   src/ffmpeg/traits.cpp
)

target_link_libraries(ffmpeg_helper
   PUBLIC
      ffmpeg::ffmpeg spdlog::spdlog
)

target_include_directories(
   ffmpeg_helper

   PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
   PUBLIC $<BUILD_INTERFACE:${OCS_GENERATED_INCLUDE_DIR}>
)

set_target_properties(
   ffmpeg_helper PROPERTIES

   OUTPUT_NAME ffmpeg-helper

   CXX_STANDARD 17
   CXX_STANDARD_REQUIRED YES
   CXX_EXTENSIONS NO
)

################################################################################
### Library
find_package(SQLiteBurrito CONFIG REQUIRED)

add_library(
   ocr_common STATIC

   src/common/database.cpp
   src/common/video.cpp
)

target_link_libraries(ocr_common
   PUBLIC
      ffmpeg_helper
      spdlog::spdlog SQLiteBurrito::library indicators::indicators
)

target_include_directories(
   ocr_common

   PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
   PUBLIC $<BUILD_INTERFACE:${OCS_GENERATED_INCLUDE_DIR}>
)

set_target_properties(
   ocr_common PROPERTIES

   OUTPUT_NAME ocr-common

   CXX_STANDARD 17
   CXX_STANDARD_REQUIRED YES
   CXX_EXTENSIONS NO
)

################################################################################
### Main Tool
find_package(PNG CONFIG REQUIRED)
find_package(Tesseract CONFIG REQUIRED)
find_package(lyra CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS filesystem date_time)

add_executable(
   ocr_suite

   src/recognition/bmp.cpp
   src/recognition/ocr.cpp

   src/recognition/speed_meter.cpp
   src/recognition/options.cpp

   src/recognition/main.cpp
)

target_link_libraries(ocr_suite
   PRIVATE
      ocr_common
      Tesseract::libtesseract bfg::lyra
      Boost::filesystem
)

set_target_properties(
   ocr_suite PROPERTIES

   OUTPUT_NAME ocr-suite

   CXX_STANDARD 17
   CXX_STANDARD_REQUIRED YES
   CXX_EXTENSIONS NO
)

################################################################################
### UI Tool

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
   stb

   GIT_REPOSITORY https://github.com/nothings/stb
   GIT_TAG 8b5f1f37b5b75829fc72d38e7b5d4bcbf8a26d55
)
if (NOT stb_POPULATED)
   FetchContent_Populate(stb)
endif ()

# UI Dependencies
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(
   ocr_suite_viewer

   src/viewer/main.cpp
   src/viewer/options.cpp
   src/viewer/search.cpp
   src/viewer/results.cpp

   src/viewer/render/frontend.cpp
   src/viewer/render/backend.cpp
   src/viewer/render/impl/opengl2.cpp
   src/viewer/render/impl/opengl3.cpp
   src/viewer/render/impl/gles2.cpp
   src/viewer/render/impl/gles3.cpp
   src/viewer/render/imgui/imgui_impl_sdl_es2.cpp
   src/viewer/render/imgui/imgui_impl_sdl_es3.cpp
   src/viewer/render/imgui/std_input_text.cpp
   src/viewer/render/window.cpp

   src/viewer/views/frame_view.cpp
   src/viewer/views/search_results_view.cpp
   src/viewer/views/search_view.cpp
   src/viewer/views/viewer.cpp

   ${CMAKE_CURRENT_BINARY_DIR}/bindings/imgui_impl_opengl2.cpp
   ${CMAKE_CURRENT_BINARY_DIR}/bindings/imgui_impl_opengl3.cpp
   ${CMAKE_CURRENT_BINARY_DIR}/bindings/imgui_impl_sdl.cpp
)

target_link_libraries(ocr_suite_viewer
   PRIVATE
      ocr_common
      SDL2::SDL2-static SDL2::SDL2main imgui::imgui glad::glad OpenGL::GL
      Boost::date_time Boost::filesystem bfg::lyra
)

target_include_directories(
   ocr_suite_viewer PUBLIC

   PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/bindings>
   PUBLIC $<BUILD_INTERFACE:${stb_SOURCE_DIR}>
   PRIVATE src/viewer/fonts
)

set_target_properties(
   ocr_suite_viewer PROPERTIES

   OUTPUT_NAME ocr-viewer

   CXX_STANDARD 17
   CXX_STANDARD_REQUIRED YES
   CXX_EXTENSIONS NO
)

################################################################################
### Tests and installers

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
