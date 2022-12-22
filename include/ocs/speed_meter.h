//
// Created by Dennis Sitelew on 22.12.22.
//

#ifndef OCR_SUITE_SPEED_METER_H
#define OCR_SUITE_SPEED_METER_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>

namespace ocs {

class speed_meter {
public:
   using clock_t = std::chrono::steady_clock;
   using time_point_t = clock_t::time_point;

   struct progress {
      //! Recognition rate in FPS
      double recognized_frames_per_second{};

      //! Processing rate in FPS
      double total_frames_per_second{};

      //! Last frame number
      std::int64_t last_frame_number{};
   };

   using callback_t = std::function<void(const progress &)>;

public:
   speed_meter(std::int64_t starting_frame_num, callback_t cb);

public:
   void add_ocr_frame(std::int64_t num);
   void add_skipped_frame(std::int64_t num);

private:
   void check_progress();

private:
   std::mutex mutex_{};
   callback_t cb_;

   time_point_t last_report_time_{clock_t::now()};
   std::size_t frames_processed_{0};
   std::int64_t last_frame_number_{0};
   std::int64_t last_print_frame_number_{0};
};

} // namespace ocs

#endif // OCR_SUITE_SPEED_METER_H
