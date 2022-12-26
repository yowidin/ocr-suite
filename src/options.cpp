//
// Created by Dennis Sitelew on 22.12.22.
//

#include <ocs/options.h>
#include <ocs/video.h>

#include <boost/filesystem.hpp>
#include <lyra/lyra.hpp>

#include <thread>

using namespace ocs;

namespace {

std::string get_default_tess_data_path(const char *app_path) {
   boost::filesystem::path path{app_path};
   path.remove_filename();
   path /= "tessdata";
   return path.string();
}

} // namespace

std::optional<options> options::parse(int argc, const char **argv) {
   options result;
   result.ocr_threads = std::thread::hardware_concurrency();
   result.language = "eng+rus+deu";
   result.frame_filter = static_cast<std::uint16_t>(video::frame_filter::I_and_P);
   result.tess_data_path = get_default_tess_data_path(argv[0]);

   bool show_help{false};

   auto cli =
       lyra::opt(result.ocr_threads, "num_threads")["-p"]["--num-threads"]("Number of threads to use for OCR") |
       lyra::opt(result.language, "language")["-l"]["--language"]("OCR language, e.g. 'eng+rus+deu', or just 'eng'") |
       lyra::opt(result.tess_data_path, "tess_data_path")["-t"]["--tess-data-path"](
           "Path to the Tesseract data directory. You can download a copy here: "
           "https://github.com/tesseract-ocr/tessdata/releases/") |
       lyra::opt(result.frame_filter, "frame_filter")["-f"]["--frame-filter"](
           "Video frame filter. 1 for I-frames, 2 for P-frames, and 4 for B-frames, can be a combination of multiple "
           "frame types. 3 is default (I+P frames)") |
       lyra::opt(result.video_file, "video_file")["-i"]["--video-file"]("Video file to process").required() |
       lyra::opt(result.database_file, "database_file")["-o"]["--database-file"]("Resulting OCR database").required() |
       lyra::opt([&](bool) { result.save_bitmaps = true; })["-b"]["--save-bitmaps"](
           "Save video bitmaps in the out/ subdirectory") |
       lyra::help(show_help);

   auto parse_result = cli.parse({argc, argv});
   if (!parse_result) {
      std::cerr << "Error in command line: " << parse_result.message() << std::endl;
      std::cerr << cli << std::endl;
      return std::nullopt;
   }

   if (!boost::filesystem::exists(result.video_file)) {
      std::cerr << "Video file does not exist: " << result.video_file << std::endl;
      return std::nullopt;
   }

   if (!boost::filesystem::exists(result.tess_data_path)) {
      std::cerr << "Tesseract data directory does not exist: " << result.tess_data_path << std::endl;
      return std::nullopt;
   }

   if (show_help) {
      std::cerr << cli << std::endl;
      return {};
   }

   return result;
}