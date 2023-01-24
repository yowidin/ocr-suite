//
// Created by Dennis Sitelew on 14.01.23.
//

#include <ocs/viewer/options.h>

#include <boost/filesystem.hpp>
#include <lyra/lyra.hpp>

using namespace ocs::viewer;

std::optional<options> options::parse(int argc, const char **argv) {
   options result;

   bool show_help{false};

   auto cli =
       lyra::opt(result.video_dir, "video_dir")["-i"]["--video-dir"]("Video files directory").required() |
       lyra::opt(result.video_extension, "video_ext")["-v"]["--video-ext"]("Video file extension, e.g.: .mkv")
           .required() |
       lyra::opt(result.db_extension, "db_ext")["-d"]["--db-ext"]("Database file extension, e.g: .db").required() |
       lyra::help(show_help);

   auto parse_result = cli.parse({argc, argv});
   if (!parse_result) {
      std::cerr << "Error in command line: " << parse_result.message() << std::endl;
      std::cerr << cli << std::endl;
      return std::nullopt;
   }

   if (!boost::filesystem::exists(result.video_dir) || !boost::filesystem::is_directory(result.video_dir)) {
      std::cerr << "Video files directory does not exist: " << result.video_dir << std::endl;
      return std::nullopt;
   }

   if (show_help) {
      std::cerr << cli << std::endl;
      return {};
   }

   return result;
}
