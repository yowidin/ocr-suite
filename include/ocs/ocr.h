//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCR_SUITE_OCR_H
#define OCR_SUITE_OCR_H

#include <ocs/video.h>
#include <functional>
#include <string>
#include <vector>

#include <tesseract/baseapi.h>

namespace ocs {

class ocr {
public:
   using value_queue_t = video::queue_t;
   using value_queue_ptr_t = video::queue_ptr_t;
   using frame_t = value_queue_t::value_ptr_t;

   struct text_entry {
      int left, top, right, bottom;
      float confidence;
      std::string text;
   };

   struct ocr_result {
      std::int64_t frame_number{};
      std::vector<text_entry> entries{};
   };

   using ocr_result_cb_t = std::function<void(const ocr_result &)>;
   using ocr_filter_cb_t = std::function<bool(std::int64_t)>;

public:
   ocr(const std::string &tess_data_path, const std::string &languages, ocr_result_cb_t cb);
   ocr(const ocr &) = delete;

   ocr &operator=(const ocr &) = delete;

public:
   void start(const value_queue_ptr_t &queue, const ocr_filter_cb_t &filter);

private:
   void do_ocr(const frame_t &frame);

private:
   tesseract::TessBaseAPI ocr_api_{};
   ocr_result_cb_t cb_;
   int min_letters_threshold_{3};
};

} // namespace ocs

#endif // OCR_SUITE_OCR_H
