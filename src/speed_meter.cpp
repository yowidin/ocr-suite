//
// Created by Dennis Sitelew on 22.12.22.
//

#include <ocs/speed_meter.h>

using namespace ocs;

using namespace std;
using namespace std::chrono;

speed_meter::speed_meter(int64_t starting_frame_num, callback_t cb)
   : cb_{std::move(cb)}
   , last_print_frame_number_{starting_frame_num} {
   // Nothing to do here
}

void speed_meter::add_ocr_frame(std::int64_t num) {
   std::lock_guard lock{mutex_};

   last_frame_number_ = std::max(last_frame_number_, num);
   ++frames_processed_;

   check_progress();
}

void speed_meter::add_skipped_frame(std::int64_t num) {
   std::lock_guard lock{mutex_};

   last_frame_number_ = std::max(last_frame_number_, num);

   check_progress();
}

void speed_meter::check_progress() {
   constexpr auto report_interval = 5s;

   const auto now = clock_t::now();
   const auto elapsed = now - last_report_time_;
   if (elapsed < report_interval) {
      return;
   }

   auto elapsed_ms = static_cast<double>(duration_cast<milliseconds>(elapsed).count());

   auto frames_seeked = last_frame_number_ - last_print_frame_number_;

   progress report{};
   report.recognized_frames_per_second = (static_cast<double>(frames_processed_) / elapsed_ms) * 1000.0;
   report.total_frames_per_second = (static_cast<double>(frames_seeked) / elapsed_ms) * 1000.0;
   report.last_frame_number = last_frame_number_;
   cb_(report);

   last_print_frame_number_ = last_frame_number_;
   last_report_time_ = now;
   frames_processed_ = 0;
}
