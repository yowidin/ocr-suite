//
// Created by Dennis Sitelew on 22.12.22.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <lyra/lyra.hpp>

#include <ocs/config.h>
#include <ocs/recognition/provider/tesseract.h>

#if OCS_VISION_KIT_SUPPORT()
#include <ocs/recognition/provider/vision_kit.h>
#endif // OCS_VISION_KIT_SUPPORT()

namespace ocs::recognition {

struct options {
   explicit options(lyra::cli &cli);

   static std::optional<options> parse(int argc, const char **argv);

   lyra::group global{};
   lyra::group subcommands{};

   //! Number of OCR threads
   std::uint16_t ocr_threads{};

   //! Video file name
   std::string video_file{};

   //! Database file name
   std::string database_file{};

   //! Tesseract options
   provider::tesseract::config tesseract{subcommands};

#if OCS_VISION_KIT_SUPPORT()
   provider::vision_kit::config vision_kit{subcommands};
#endif // OCS_VISION_KIT_SUPPORT()

   //! Video frame filter
   std::uint16_t frame_filter{};

   //! Save bitmaps to disk
   bool save_bitmaps{false};
};

} // namespace ocs::recognition
