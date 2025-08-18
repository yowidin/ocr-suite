/**
 * @file   vision_kit.cpp
 * @author Dennis Sitelew
 * @date   Aug. 17, 2025
 */

#include <ocs/common/util.h>
#include <ocs/recognition/provider/vision_kit.h>

#include <spdlog/spdlog.h>

#include <utility>

using namespace ocs;
namespace provider = recognition::provider;

/*******************************************************************************
 * Swift implementation
 ******************************************************************************/
extern "C" {
typedef void (*recognition_cb_t)(std::uint32_t frame_number,
                                 std::uint32_t count,
                                 char **texts,
                                 std::uint32_t *lefts,
                                 std::uint32_t *tops,
                                 std::uint32_t *rights,
                                 std::uint32_t *bottoms,
                                 float *confidences,
                                 char *error,
                                 void *user_data);

void vk_do_ocr(uint32_t frame_number,
               const char *languages,
               const uint8_t *data,
               std::int32_t w,
               std::int32_t h,
               std::int32_t bpl,
               recognition_cb_t callback,
               void *user_data);

void vk_free_ocr_results(std::uint32_t count,
                         char **texts,
                         std::uint32_t *lefts,
                         std::uint32_t *tops,
                         std::uint32_t *rights,
                         std::uint32_t *bottoms,
                         float *confidences,
                         char *error);
} // extern "C"

/*******************************************************************************
 * Tesseract provider config
 ******************************************************************************/
provider::vision_kit::config::config(lyra::group &cli) {
   // TODO: Get a way to query available languages
   cli.add_argument(lyra::command("vkit", [&](const lyra::group &f) { selected = true; })
                        .help("Recognize using the Apple's Vision Kit")
                        .add_argument(lyra::opt(languages, "languages")
                                          .name("-l")
                                          .name("--languages")
                                          .help("OCR languages, e.g. 'en-US+ru-RU+de-DE', or just 'en-US'")));
}

bool provider::vision_kit::config::validate() {
   return true;
}

/*******************************************************************************
 * VisionKit provider
 ******************************************************************************/
provider::vision_kit::vision_kit(config cfg)
   : config_{std::move(cfg)} {
}

provider::vision_kit::~vision_kit() = default;

provider::provider::result_t provider::vision_kit::do_ocr(const ffmpeg::decoder::frame &frame) {
   promise_ = std::promise<result_t>();

   vk_do_ocr(frame.frame_number, config_.languages.c_str(), frame.data.data(), frame.width, frame.height,
             frame.bytes_per_line, vision_kit::handle_ocr_results, this);

   // Wait until OCR is done
   auto future = promise_.get_future();

   return future.get();
}

void provider::vision_kit::handle_ocr_results(std::uint32_t frame_number,
                                              std::uint32_t count,
                                              char **texts,
                                              std::uint32_t *lefts,
                                              std::uint32_t *tops,
                                              std::uint32_t *rights,
                                              std::uint32_t *bottoms,
                                              float *confidences,
                                              char *error,
                                              void *user_data) {
   auto self = static_cast<vision_kit *>(user_data);

   std::string error_message{};
   if (error) {
      error_message = error;
   }

   common::ocr_result result{};

   if (!error) {
      result.frame_number = frame_number;
      result.entries.reserve(count);
      for (std::uint32_t i = 0; i < count; ++i) {
         std::string text = texts[i];
         if (text.length() < 3) {
            continue;
         }

         auto &entry = result.entries.emplace_back();
         entry.text = text;
         entry.left = static_cast<int>(lefts[i]);
         entry.top = static_cast<int>(tops[i]);
         entry.right = static_cast<int>(rights[i]);
         entry.bottom = static_cast<int>(bottoms[i]);
         entry.confidence = confidences[i];
      }
   }

   vk_free_ocr_results(count, texts, lefts, tops, rights, bottoms, confidences, error);

   if (!error_message.empty()) {
      spdlog::warn("OCR error: {}", error_message);
      self->promise_.set_value(std::nullopt);
   } else {
      self->promise_.set_value(result);
   }
}