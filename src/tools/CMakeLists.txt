find_package(nlohmann_json REQUIRED)

add_executable(ocr_cli
    ${CMAKE_SOURCE_DIR}/src/recognition/provider/tesseract.cpp

    ocr-cli.cpp
)

target_link_libraries(ocr_cli
    PRIVATE ocr_common
    PRIVATE Boost::filesystem bfg::lyra Tesseract::libtesseract nlohmann_json::nlohmann_json
    PRIVATE stb::stb
)

set_target_properties(ocr_cli PROPERTIES OUTPUT_NAME ocr-cli)

################################################################################
### OCR results viewer
add_executable(ocr_results_viewer
    ocr-results-viewer.cpp
)

target_link_libraries(ocr_results_viewer
    PRIVATE ocr_common ocr_ui_common
    PRIVATE Boost::date_time Boost::filesystem bfg::lyra stb::stb nlohmann_json::nlohmann_json
)

set_target_properties(ocr_results_viewer PROPERTIES OUTPUT_NAME ocr-results-viewer)