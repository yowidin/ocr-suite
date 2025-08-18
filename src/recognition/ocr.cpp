//
// Created by Dennis Sitelew on 21.12.22.
//

#include <ocs/recognition/bmp.h>
#include <ocs/recognition/ocr.h>
#include <ocs/recognition/provider/tesseract.h>

#include <functional>

#include <spdlog/spdlog.h>
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
   // TODO: Provider selection
   provider_ = std::make_unique<provider::tesseract>(opts_->tesseract);
   if (opts_->tesseract.selected) {
      provider_ = std::make_unique<provider::tesseract>(opts_->tesseract);
   } else {
      throw std::runtime_error("No OCR provider selected");
   }

   bitmap_directory_ = get_bitmap_directory(opts_->database_file);
   if (opts_->save_bitmaps) {
      boost::filesystem::create_directories(bitmap_directory_);
   }
}

ocr::~ocr() = default;

void ocr::start(const value_queue_ptr_t &queue, const ocr_filter_cb_t &filter) const {
   while (true) {
      auto opt_frame = queue->get_consumer_value();
      if (!opt_frame.has_value()) {
         return;
      }

      auto frame = std::move(opt_frame.value());

      if (opts_->save_bitmaps) {
         const auto file_name = get_frame_path(bitmap_directory_, frame->frame_number);
         bmp::save_image(frame->data, frame->width, frame->height, file_name);
      }

      if (filter(frame->frame_number)) {
         if (const auto result = provider_->do_ocr(*frame)) {
            cb_(*result);
         }
      }

      queue->add_producer_value(frame);
   }
}
