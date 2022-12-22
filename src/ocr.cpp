//
// Created by Dennis Sitelew on 21.12.22.
//

#include <ocs/ocr.h>
#include <spdlog/spdlog.h>

#include <tesseract/baseapi.h>

using namespace ocs;

ocr::ocr(const std::string &tess_data_path, const std::string &languages, ocr_result_cb_t cb)
   : cb_{std::move(cb)} {
   int res = ocr_api_.Init(tess_data_path.c_str(), languages.c_str(), tesseract::OEM_LSTM_ONLY);
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
}

void ocr::start(const value_queue_ptr_t &queue, const ocr_filter_cb_t &filter) {
   while (true) {
      auto opt_frame = queue->get_consumer_value();
      if (!opt_frame.has_value()) {
         return;
      }

      auto frame = opt_frame.value();
      if (filter(frame->frame_number)) {
         do_ocr(frame);
      }

      // auto file_name = std::string("out/out-") + std::to_string(frame->frame_number) + ".bmp";
      // ocs::bmp::save_image(frame->data, frame->width, frame->height, file_name);

      queue->add_producer_value(frame);
   }
}

void ocr::do_ocr(const frame_t &frame) {
   ocr_api_.SetImage(frame->data.data(), frame->width, frame->height, 3, frame->bytes_per_line);
   if (ocr_api_.Recognize(nullptr) != 0) {
      SPDLOG_ERROR("Could not recognize frame #{}", frame->frame_number);
      return;
   }

   auto it = ocr_api_.GetIterator();
   if (!it) {
      SPDLOG_ERROR("Error getting recognition results for frame #{}", frame->frame_number);
      return;
   }

   ocr_result result{};
   result.frame_number = frame->frame_number;

   for (it->Begin(); it->Next(tesseract::RIL_WORD);) {
      text_entry entry{};

      if (!it->BoundingBox(tesseract::RIL_WORD, &entry.left, &entry.top, &entry.right, &entry.bottom)) {
         continue;
      }

      entry.confidence = it->Confidence(tesseract::RIL_WORD);

      auto text = it->GetUTF8Text(tesseract::RIL_WORD);
      entry.text = std::string(text);
      delete[] text;

      if (entry.text.size() < min_letters_threshold_) {
         continue;
      }
      result.entries.push_back(std::move(entry));
   }

   delete it;

   cb_(result);
}
