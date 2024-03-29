//
// Created by Dennis Sitelew on 21.12.22.
//

#include <ocs/recognition/bmp.h>
#include <ocs/recognition/ocr.h>

#include <ocs/common/util.h>

#include <functional>

#include <spdlog/spdlog.h>
#include <tesseract/baseapi.h>
#include <boost/filesystem.hpp>

using namespace ocs::recognition;

namespace {

std::string get_bitmap_directory(const std::string &db_path) {
   boost::filesystem::path path{db_path};
   path.remove_filename();
   path /= "out";
   return path.string();
}

std::string get_frame_path(const std::string &bitmap_dir, std::int64_t frame_number) {
   boost::filesystem::path path{bitmap_dir};
   path /= fmt::format("out-{}.bmp", frame_number);
   return path.string();
}

} // namespace

ocr::ocr(const options &opts, ocr_result_cb_t cb)
   : opts_{&opts}
   , cb_{std::move(cb)} {
   int res = ocr_api_.Init(opts_->tess_data_path.c_str(), opts_->language.c_str(), tesseract::OEM_LSTM_ONLY);
   if (res) {
      throw std::runtime_error("Could not initialize tesseract");
   }

   ocr_api_.SetPageSegMode(tesseract::PageSegMode::PSM_SPARSE_TEXT);

#ifdef _WIN32
   const char *null_device = "nul";
#else
   const char *null_device = "/dev/null";
#endif

   ocr_api_.SetVariable("debug_file", null_device);

   bitmap_directory_ = get_bitmap_directory(opts_->database_file);
   if (opts_->save_bitmaps) {
      boost::filesystem::create_directories(bitmap_directory_);
   }
}

void ocr::start(const value_queue_ptr_t &queue, const ocr_filter_cb_t &filter) {
   while (true) {
      auto opt_frame = queue->get_consumer_value();
      if (!opt_frame.has_value()) {
         return;
      }

      auto frame = opt_frame.value();

      if (opts_->save_bitmaps) {
         const auto file_name = get_frame_path(bitmap_directory_, frame->frame_number);
         bmp::save_image(frame->data, frame->width, frame->height, file_name);
      }

      if (filter(frame->frame_number)) {
         do_ocr(frame);
      }

      queue->add_producer_value(frame);
   }
}

void ocr::do_ocr(const frame_t &frame) {
   ocr_api_.SetImage(frame->data.data(), frame->width, frame->height, 3, frame->bytes_per_line);
   if (ocr_api_.Recognize(nullptr) != 0) {
      spdlog::error("Could not recognize frame #{}", frame->frame_number);
      return;
   }

   auto it = ocr_api_.GetIterator();
   if (!it) {
      spdlog::error("Error getting recognition results for frame #{}", frame->frame_number);
      return;
   }

   common::ocr_result result{};
   result.frame_number = frame->frame_number;

   for (it->Begin(); it->Next(tesseract::RIL_WORD);) {
      common::text_entry entry{};

      if (!it->BoundingBox(tesseract::RIL_WORD, &entry.left, &entry.top, &entry.right, &entry.bottom)) {
         continue;
      }

      entry.confidence = it->Confidence(tesseract::RIL_WORD);

      auto text = it->GetUTF8Text(tesseract::RIL_WORD);
      entry.text = std::string(text);
      delete[] text;

      common::util::trim(entry.text);

      if (entry.text.size() < min_letters_threshold_) {
         continue;
      }
      result.entries.push_back(std::move(entry));
   }

   delete it;

   cb_(result);
}
