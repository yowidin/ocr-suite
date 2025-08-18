cmake_minimum_required(VERSION 3.10)
project(ocr_suite VERSION 0.4.0 LANGUAGES CXX)

################################################################################
### Common C++ configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#################################################################################
### Setup platform-specific flags
set(OCS_WINDOWS OFF)
set(OCS_APPLE OFF)
set(OCS_UNIX OFF)

if(APPLE)
    set(OCS_APPLE ON)
elseif(UNIX)
    set(OCS_UNIX ON)
elseif(WIN32)
    set(OCS_WINDOWS ON)
endif()

message(STATUS "Windows: ${OCS_WINDOWS}")
message(STATUS "Apple: ${OCS_APPLE}")
message(STATUS "Unix: ${OCS_UNIX}")

if(OCS_APPLE)
    option(OCS_VISION_KIT_SUPPORT "Add support for macOS native OCR using the VisionKit" ON)
    message(STATUS "VisionKit: ${OCS_VISION_KIT_SUPPORT}")
endif()

set(OCS_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(OCS_GENERATED_INCLUDE_DIR "${OCS_GENERATED_DIR}/include")
set(OCS_GENERATED_CONFIG_HEADER "${OCS_GENERATED_INCLUDE_DIR}/ocs/config.h")
configure_file(cmake/config.h.in ${OCS_GENERATED_CONFIG_HEADER} @ONLY)

################################################################################
### Dependencies
find_package(ffmpeg CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(indicators CONFIG REQUIRED)
find_package(SQLiteBurrito CONFIG REQUIRED)
find_package(PNG CONFIG REQUIRED)
find_package(Tesseract CONFIG REQUIRED)
find_package(lyra CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS filesystem date_time)
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

if(OCS_VISION_KIT_SUPPORT)
    enable_language(Swift)

    find_library(VISION_FRAMEWORK Vision)
    find_library(CORE_IMAGE_FRAMEWORK CoreImage)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
endif()

################################################################################
### FFMPEG helper
add_library(ffmpeg_helper STATIC
    src/ffmpeg/decoder.cpp
    src/ffmpeg/traits.cpp
)

target_link_libraries(ffmpeg_helper PUBLIC ffmpeg::ffmpeg spdlog::spdlog)

target_include_directories(ffmpeg_helper
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${OCS_GENERATED_INCLUDE_DIR}>
)

set_target_properties(ffmpeg_helper PROPERTIES OUTPUT_NAME ffmpeg-helper)

################################################################################
### Library
add_library(ocr_common STATIC
    src/common/database.cpp
    src/common/video.cpp
)

target_link_libraries(ocr_common
    PUBLIC ffmpeg_helper spdlog::spdlog SQLiteBurrito::library indicators::indicators
)

target_include_directories(ocr_common
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${OCS_GENERATED_INCLUDE_DIR}>
)

set_target_properties(ocr_common PROPERTIES OUTPUT_NAME ocr-common)

################################################################################
### OCR library
add_library(ocr_recognition_common STATIC
    src/recognition/provider/tesseract.cpp
)

target_link_libraries(ocr_recognition_common
    PUBLIC ocr_common
    PUBLIC Tesseract::libtesseract bfg::lyra Boost::filesystem spdlog::spdlog
)

if(OCS_VISION_KIT_SUPPORT)
    target_sources(ocr_recognition_common
        PRIVATE src/recognition/provider/vision_kit.swift
        PRIVATE src/recognition/provider/vision_kit.cpp
    )

    set_target_properties(ocr_recognition_common PROPERTIES
        Swift_LANGUAGE_VERSION 4
    )

    target_link_libraries(ocr_recognition_common
        PUBLIC ${VISION_FRAMEWORK}
        PUBLIC ${CORE_IMAGE_FRAMEWORK}
        PUBLIC ${FOUNDATION_FRAMEWORK}
        PUBLIC ${CORE_GRAPHICS_FRAMEWORK}
    )
endif()

################################################################################
### Main Tool
add_executable(ocr_suite
    src/recognition/bmp.cpp
    src/recognition/ocr.cpp

    src/recognition/speed_meter.cpp
    src/recognition/options.cpp

    src/recognition/main.cpp
)

target_link_libraries(ocr_suite
    PRIVATE ocr_recognition_common
)

set_target_properties(ocr_suite PROPERTIES OUTPUT_NAME ocr-suite)

################################################################################
### UI Common
add_library(ocr_ui_common STATIC
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

    ${CMAKE_CURRENT_BINARY_DIR}/bindings/imgui_impl_opengl2.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/bindings/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/bindings/imgui_impl_sdl.cpp
)

target_link_libraries(ocr_ui_common
    PUBLIC SDL2::SDL2-static SDL2::SDL2main imgui::imgui glad::glad OpenGL::GL
)

target_include_directories(ocr_ui_common
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/bindings>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    PUBLIC $<BUILD_INTERFACE:${OCS_GENERATED_INCLUDE_DIR}>
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/viewer/fonts>
)

set_target_properties(ocr_ui_common PROPERTIES OUTPUT_NAME ocr-ui-common)

################################################################################
### UI Tool
add_executable(ocr_suite_viewer
    src/viewer/main.cpp
    src/viewer/options.cpp
    src/viewer/search.cpp
    src/viewer/results.cpp

    src/viewer/views/frame_view.cpp
    src/viewer/views/search_results_view.cpp
    src/viewer/views/search_view.cpp
    src/viewer/views/viewer.cpp
)

target_link_libraries(ocr_suite_viewer
    PRIVATE ocr_common ocr_ui_common
    PRIVATE Boost::date_time Boost::filesystem bfg::lyra stb::stb
)

set_target_properties(ocr_suite_viewer PROPERTIES OUTPUT_NAME ocr-viewer)

################################################################################
### Tools
add_subdirectory(src/tools)

################################################################################
### Tests and installers
# Testing
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

include(cmake/install_helper.cmake)
include(cmake/package_helper.cmake)
