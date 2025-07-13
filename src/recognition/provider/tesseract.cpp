/**
 * @file   tesseract.cpp
 * @author Dennis Sitelew
 * @date   Jul. 13, 2025
 */

#include <ocs/common/util.h>
#include <ocs/recognition/provider/tesseract.h>

#include <tesseract/baseapi.h>

#include <spdlog/spdlog.h>

using namespace ocs;
namespace provider = recognition::provider;

class provider::tesseract::api {
public:
   ::tesseract::TessBaseAPI *operator->() { return &api_; }
   ::tesseract::TessBaseAPI &operator*() { return api_; }

private:
   ::tesseract::TessBaseAPI api_{};
};

provider::tesseract::tesseract(const char *tess_data_path, const char *language)
   : api_{std::make_unique<api>()} {
   auto &api = *api_;

   if (const auto res = api->Init(tess_data_path, language, ::tesseract::OEM_LSTM_ONLY)) {
      throw std::runtime_error(fmt::format("Could not initialize tesseract: {}", res));
   }

   api->SetPageSegMode(::tesseract::PageSegMode::PSM_SPARSE_TEXT);

#ifdef _WIN32
   auto null_device = "nul";
#else
   auto null_device = "/dev/null";
#endif

   api->SetVariable("debug_file", null_device);
}

provider::tesseract::~tesseract() = default;

provider::provider::result_t provider::tesseract::do_ocr(const ffmpeg::decoder::frame &frame) {
   auto &api = *api_;

   api->SetImage(frame.data.data(), frame.width, frame.height, 3, frame.bytes_per_line);
   if (api->Recognize(nullptr) != 0) {
      spdlog::error("Could not recognize frame #{}", frame.frame_number);
      return std::nullopt;
   }

   const std::unique_ptr<::tesseract::ResultIterator> it{api->GetIterator()};
   if (!it) {
      spdlog::error("Error getting recognition results for frame #{}", frame.frame_number);
      return std::nullopt;
   }

   common::ocr_result result{};
   result.frame_number = frame.frame_number;

   constexpr auto level = ::tesseract::RIL_WORD;
   for (it->Begin(); it->Next(level);) {
      common::text_entry entry{};

      if (!it->BoundingBox(level, &entry.left, &entry.top, &entry.right, &entry.bottom)) {
         continue;
      }

      entry.confidence = it->Confidence(level);

      std::unique_ptr<const char[]> text{it->GetUTF8Text(level)};
      entry.text = std::string(text.get());

      common::util::trim(entry.text);

      if (entry.text.size() < min_letters_threshold_) {
         continue;
      }
      result.entries.push_back(std::move(entry));
   }

   return result;
}