/**
 * @file   ocr-cli.cpp
 * @author Dennis Sitelew
 * @date   Jul. 17, 2025
 */

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION

#include <memory>
#include <optional>
#include <string>

#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <boost/filesystem.hpp>
#include <lyra/lyra.hpp>
#include <nlohmann/json.hpp>

#include <ocs/config.h>
#include <ocs/recognition/provider/tesseract.h>

#if OCS_VISION_KIT_SUPPORT()
#include <ocs/recognition/provider/vision_kit.h>
#endif // OCS_VISION_KIT_SUPPORT()

namespace provider_ns = ocs::recognition::provider;

namespace ocs::cli {

struct options {
   explicit options(lyra::cli &cli) {
      global.add_argument(lyra::help(show_help));
      global.add_argument(lyra::opt(output_type, "output_type")
                              .name("-o")
                              .name("--output-type")
                              .choices("json", "raw")
                              .help("How to output the recognized text (raw or json)."));
      global.add_argument(lyra::arg(image_file, "image_path").required().help("Image path to perform OCR on."));

      subcommands.require(1, 1);

      cli.add_argument(subcommands).add_argument(global);
   }

   static std::optional<options> parse(int argc, const char **argv) {
      auto cli = lyra::cli{};

      options result{cli};

      if (const auto parse_result = cli.parse({argc, argv}); !parse_result) {
         std::cerr << "Error in command line: " << parse_result.message() << std::endl;
         std::cerr << cli << std::endl;
         return std::nullopt;
      }

      if (result.show_help) {
         std::cerr << cli << std::endl;
         return {};
      }

      if (!boost::filesystem::exists(result.image_file)) {
         std::cerr << "Image file does not exist: " << result.image_file << std::endl;
         return std::nullopt;
      }

      if (result.tesseract.selected && !result.tesseract.validate()) {
         return std::nullopt;
      }

#if OCS_VISION_KIT_SUPPORT()
      if (result.vision_kit.selected && !result.vision_kit.validate()) {
         return std::nullopt;
      }
#endif // OCS_VISION_KIT_SUPPORT()

      return result;
   }

   lyra::group global{};
   lyra::group subcommands{};
   provider_ns::tesseract::config tesseract{subcommands};

#if OCS_VISION_KIT_SUPPORT()
   provider_ns::vision_kit::config vision_kit{subcommands};
#endif // OCS_VISION_KIT_SUPPORT()

   bool show_help{false};
   std::string image_file{};
   std::string output_type{"raw"};
};

ffmpeg::decoder::frame load_frame(const std::string &image_path) {
   int x, y, n;
   const auto data = stbi_load(image_path.c_str(), &x, &y, &n, 0);
   if (!data) {
      throw std::runtime_error(fmt::format("Failed to load image: {}", stbi_failure_reason()));
   }

   ffmpeg::decoder::frame result;

   const auto total_size = x * y * n;

   result.width = x;
   result.height = y;
   result.frame_number = 0;
   result.bytes_per_line = n * x;

   result.data.resize(total_size);
   std::copy_n(data, total_size, std::begin(result.data));

   stbi_image_free(data);

   return result;
}

} // namespace ocs::cli

std::ostream &operator<<(std::ostream &os, ocs::common::text_entry const &entry) {
   os << fmt::format("{},{},{},{},{},{:.2f}", entry.text, entry.left, entry.top, entry.right, entry.bottom,
                     entry.confidence);
   return os;
}

void print_raw(const ocs::common::ocr_result &res) {
   for (const auto &entry : res.entries) {
      std::cout << entry << std::endl;
   }
}

void print_json(const ocs::cli::options &opts, const ocs::common::ocr_result &res) {
   auto truncate_to = [](auto value, auto places) {
      const auto multiplier = std::pow(static_cast<decltype(value)>(10), places);
      return std::floor(value * multiplier) / multiplier;
   };

   using json = nlohmann::json;

   auto root = json::object();
   root["file"] = opts.image_file;

   auto entries = json::array();
   for (const auto &e : res.entries) {
      auto position = json::object();
      position["left"] = e.left;
      position["top"] = e.top;
      position["right"] = e.right;
      position["bottom"] = e.bottom;

      auto entry = json::object();

      entry["text"] = e.text;
      entry["position"] = position;
      entry["confidence"] = truncate_to(e.confidence, 2);

      entries.push_back(entry);
   }

   root["entries"] = entries;

   std::cout << root << std::endl;
}

int main(const int argc, const char **argv) {
   const auto maybe_options = ocs::cli::options::parse(argc, argv);

   if (!maybe_options) {
      return EXIT_FAILURE;
   }

   const auto &options = *maybe_options;

   try {
      const auto frame = ocs::cli::load_frame(options.image_file);

      std::unique_ptr<provider_ns::provider> provider;
      if (options.tesseract.selected) {
         provider = std::make_unique<provider_ns::tesseract>(options.tesseract);
#if OCS_VISION_KIT_SUPPORT()
      } else if (options.vision_kit.selected) {
         provider = std::make_unique<provider_ns::vision_kit>(options.vision_kit);
#endif // OCS_VISION_KIT_SUPPORT()
      } else {
         spdlog::error("No OCR provider selected");
         return EXIT_FAILURE;
      }

      const auto ocr_result = provider->do_ocr(frame);
      if (!ocr_result) {
         spdlog::error("OCR failed");
         return EXIT_FAILURE;
      }

      const auto &res = *ocr_result;
      if (options.output_type == "raw") {
         print_raw(res);
      } else if (options.output_type == "json") {
         print_json(options, res);
      } else {
         throw std::runtime_error("Unknown output type");
      }
   } catch (const std::exception &ex) {
      spdlog::error("{}", ex.what());
      return EXIT_FAILURE;
   } catch (...) {
      spdlog::error("Unexpected exception");
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}