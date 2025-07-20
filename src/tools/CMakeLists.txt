find_package(nlohmann_json REQUIRED)

add_executable(ocr_cli
   ${CMAKE_SOURCE_DIR}/src/recognition/provider/tesseract.cpp

   ocr-cli.cpp
)

target_link_libraries(ocr_cli
   PRIVATE
      ocr_common
      Boost::filesystem bfg::lyra Tesseract::libtesseract
      nlohmann_json::nlohmann_json stb::stb
)

set_target_properties(ocr_cli PROPERTIES
   OUTPUT_NAME ocr-cli

   CXX_STANDARD 17
   CXX_STANDARD_REQUIRED YES
   CXX_EXTENSIONS NO
)
