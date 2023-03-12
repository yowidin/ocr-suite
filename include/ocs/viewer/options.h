//
// Created by Dennis Sitelew on 14.01.23.
//

#ifndef OCR_SUITE_VIEWER_OPTIONS_H
#define OCR_SUITE_VIEWER_OPTIONS_H

#include <cstdint>
#include <optional>
#include <string>

namespace ocs::viewer {

struct options {
   static std::optional<options> parse(int argc, const char **argv);

   //! Path to the video files directory
   std::string video_dir;

   //! Video file extension
   std::string video_extension;

   //! Database file extension
   std::string db_extension;

   //! Store search results only in the RAM
   bool in_memory_results{true};
};

} // namespace ocs::viewer

#endif // OCR_SUITE_VIEWER_OPTIONS_H
