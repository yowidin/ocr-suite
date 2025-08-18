//
// Created by Dennis Sitelew on 22.12.22.
//

#include <ocs/recognition/options.h>

#include <ocs/ffmpeg/decoder.h>

#include <boost/filesystem.hpp>

#include <thread>

using namespace ocs::recognition;

namespace {

std::string get_default_tess_data_path(const char *app_path) {
   boost::filesystem::path path{app_path};
   path.remove_filename();
   path /= "tessdata";
   return path.string();
}

} // namespace

options::options(lyra::cli &cli) {
   // Nothing to do here
}

std::optional<options> options::parse(int argc, const char **argv) {
   auto cli = lyra::cli{};

   options res{cli};

   res.ocr_threads = std::thread::hardware_concurrency();
   res.frame_filter = static_cast<std::uint16_t>(ffmpeg::decoder::frame_filter::I_and_P);
   res.tesseract.data_path = get_default_tess_data_path(argv[0]);

   bool show_help{false};

   res.global.add_argument(lyra::opt(res.ocr_threads, "num_threads")
                               .name("-p")
                               .name("--num-threads")
                               .help("Number of threads to use for OCR"));

   res.global.add_argument(lyra::opt(res.frame_filter, "frame_filter")
                               .name("-f")
                               .name("--frame_filter")
                               .help("Video frame filter. 1 for I-frames, 2 for P-frames, and 4 for B-frames."
                                     "It can be a combination of multiple frame types. The default is 3 (I+P frames)"));

   res.global.add_argument(lyra::opt(res.video_file, "video_file")
                               .name("-i")
                               .name("--video-file")
                               .help("Video file to process")
                               .required());

   res.global.add_argument(lyra::opt(res.database_file, "database_file")
                               .name("-o")
                               .name("--database-file")
                               .help("Resulting OCR database")
                               .required());

   res.global.add_argument(lyra::opt([&](bool) { res.save_bitmaps = true; })
                               .name("-b")
                               .name("--save-bitmaps")
                               .help("Save video bitmaps in the out/ subdirectory"));

   res.global.add_argument(lyra::help(show_help));

   res.subcommands.require(1, 1);

   cli.add_argument(res.subcommands).add_argument(res.global);

   if (const auto parse_result = cli.parse({argc, argv}); !parse_result) {
      std::cerr << "Error in command line: " << parse_result.message() << std::endl;
      std::cerr << cli << std::endl;
      return std::nullopt;
   }

   if (show_help) {
      std::cerr << cli << std::endl;
      return {};
   }

   if (res.tesseract.selected && !res.tesseract.validate()) {
      return std::nullopt;
   }

#if OCS_VISION_KIT_SUPPORT()
   if (res.vision_kit.selected && !res.vision_kit.validate()) {
      return std::nullopt;
   }
#endif // OCS_VISION_KIT_SUPPORT()

   if (!boost::filesystem::exists(res.video_file)) {
      std::cerr << "Video file does not exist: " << res.video_file << std::endl;
      return std::nullopt;
   }

   return res;
}