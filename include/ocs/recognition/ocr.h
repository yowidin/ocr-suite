//
// Created by Dennis Sitelew on 21.12.22.
//

#pragma once

#include <ocs/common/ocr_result.h>

#include <ocs/recognition/options.h>
#include <ocs/common/video.h>

#include <functional>
#include <string>
#include <vector>

#include <tesseract/baseapi.h>

namespace ocs::recognition {

class ocr {
public:
   using value_queue_t = common::video::queue_t;
   using value_queue_ptr_t = common::video::queue_ptr_t;
   using frame_t = value_queue_t::value_ptr_t;

   using ocr_result_cb_t = std::function<void(const common::ocr_result &)>;
   using ocr_filter_cb_t = std::function<bool(std::int64_t)>;

public:
   ocr(const options &opts, ocr_result_cb_t cb);
   ocr(const ocr &) = delete;

   ocr &operator=(const ocr &) = delete;

public:
   void start(const value_queue_ptr_t &queue, const ocr_filter_cb_t &filter);

private:
   void do_ocr(const frame_t &frame);

private:
   const options *opts_;
   std::string bitmap_directory_{};

   tesseract::TessBaseAPI ocr_api_{};
   ocr_result_cb_t cb_;
   int min_letters_threshold_{3};
};

} // namespace ocs
