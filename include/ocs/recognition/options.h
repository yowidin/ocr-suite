//
// Created by Dennis Sitelew on 22.12.22.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ocs::recognition {

struct options {
   static std::optional<options> parse(int argc, const char **argv);

   //! Number of OCR threads
   std::uint16_t ocr_threads{};

   //! Video file name
   std::string video_file{};

   //! Database file name
   std::string database_file{};

   //! OCR languages
   std::string language{};

   //! Path the Tesseract data directory
   std::string tess_data_path{};

   //! Video frame filter
   std::uint16_t frame_filter{};

   //! Save bitmaps to disk
   bool save_bitmaps{false};
};

} // namespace ocs::recognition
