#include <ocs/bmp.h>
#include <ocs/database.h>
#include <ocs/options.h>
#include <ocs/speed_meter.h>

#include <cstdlib>
#include <limits>
#include <memory>
#include <thread>

#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <indicators/progress_spinner.hpp>

int main(int argc, const char **argv) {
   auto pres = ocs::options::parse(argc, argv);
   if (!pres) {
      return EXIT_FAILURE;
   }

   const auto options = pres.value();

   ocs::database db{options.database_file};
   auto queue = std::make_shared<ocs::video::queue_t>(options.ocr_threads * 2);

   /// --- Setup the progress reporters ---

   const auto starting_frame_number = db.get_starting_frame_number();
   const auto frame_filter = static_cast<ocs::video::frame_filter>(options.frame_filter);
   ocs::video video_file{options.video_file, queue, starting_frame_number, frame_filter};

   std::string postfix = "Processing ...";

   using namespace indicators;
   indicators::ProgressSpinner spinner{
       option::PostfixText{postfix},
       option::ForegroundColor{Color::yellow},
       option::SpinnerStates{std::vector<std::string>{"\\", "|", "/", "-"}},
       option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
   };

   const auto max_frames = video_file.frame_count();
   if (max_frames.has_value()) {
      spinner.set_option(option::MaxProgress{max_frames.value()});
      spinner.set_option(option::ShowPercentage{true});
   } else {
      spinner.set_option(option::MaxProgress{std::numeric_limits<size_t>::max()});
      spinner.set_option(option::ShowPercentage{false});
   }

   auto frame_number_to_time_string = [&](std::int64_t frame) {
      using namespace std::chrono;
      auto total_seconds = seconds{video_file.frame_number_to_seconds(frame)};
      const auto total_hours = duration_cast<hours>(total_seconds);
      total_seconds -= total_hours;
      const auto total_minutes = duration_cast<minutes>(total_seconds);
      total_seconds -= total_minutes;
      return fmt::format("{:02}:{:02}:{:02}", total_hours.count(), total_minutes.count(), total_seconds.count());
   };

   bool stopping = false;

   auto progress_callback = [&](const auto &report) {
      std::string time;
      if (max_frames.has_value()) {
         time = fmt::format("[{} / {}]", frame_number_to_time_string(report.last_frame_number),
                            frame_number_to_time_string(max_frames.value()));
      } else {
         time = fmt::format("[{}]", frame_number_to_time_string(report.last_frame_number));
      }

      const auto left_in_queue = queue->get_remaining_consumer_values();

      std::string text =
          fmt::format("{} {:05.2f} OCR/s, {:05.2f} seek/s, {} in queue {}", postfix,
                      report.recognized_frames_per_second, report.total_frames_per_second, left_in_queue, time);
      spinner.set_option(option::PostfixText{text});
   };

   ocs::speed_meter meter{db.get_starting_frame_number(), progress_callback};

   std::string final_text = "Done!";

   /// --- Add signal handler ---
   boost::asio::io_context ctx;
   boost::asio::signal_set signals(ctx, SIGINT, SIGTERM);
   signals.async_wait([&](const auto &ec, auto &signal) {
      if (ec) {
         SPDLOG_ERROR("Signal error: {}", ec.message());
      }

      postfix = "Stopping ...";
      spinner.set_option(option::PostfixText{postfix});
      final_text = "Interrupted!";
      stopping = true;
      queue->shutdown();
   });

   std::thread signal_thread{[&] { ctx.run(); }};

   /// --- Start the work ---
   auto consumer_func = [&]() {
      try {
         auto ocr_callback = [&](const ocs::ocr::ocr_result &result) {
            db.store(result);
            meter.add_ocr_frame(result.frame_number);
            spinner.set_progress(result.frame_number);
         };

         auto filter_callback = [&](std::int64_t frame_number) {
            auto processed = db.is_frame_processed(frame_number);
            if (processed) {
               meter.add_skipped_frame(frame_number);
               spinner.set_progress(frame_number);
            }
            return !processed;
         };

         ocs::ocr ocr{options.tess_data_path, options.language, ocr_callback};
         ocr.start(queue, filter_callback);
      } catch (const std::exception &ex) {
         SPDLOG_ERROR("Consumer thread exception: {}", ex.what());
         queue->shutdown();
         final_text = "Error!";
      } catch (...) {
         SPDLOG_ERROR("Unexpected consumer thread exception");
         queue->shutdown();
         final_text = "Error!";
      }
   };

   auto progress_message = [&](auto msg) {
     spinner.set_option(option::PostfixText{msg});
     spinner.print_progress();
     std::cout.flush();
   };

   progress_message(fmt::format("Starting {} consumer threads...", options.ocr_threads));
   std::vector<std::thread> consumers;
   for (auto i = 0; i < options.ocr_threads; ++i) {
      consumers.emplace_back(consumer_func);
   }

   bool done = false;
   try {
      progress_message("Starting decoder...");
      spinner.print_progress();
      video_file.start();
      done = true;
   } catch (const std::exception &ex) {
      SPDLOG_ERROR("Producer thread exception: {}", ex.what());
      queue->shutdown();
      final_text = "Error!";
   } catch (...) {
      SPDLOG_ERROR("Unexpected producer thread exception");
      queue->shutdown();
      final_text = "Error!";
   }

   for (auto &consumer : consumers) {
      consumer.join();
   }

   ctx.stop();
   signal_thread.join();

   int return_code = EXIT_SUCCESS;

   if (done && !stopping) {
      spinner.mark_as_completed();
      spinner.set_option(option::ForegroundColor{Color::green});
      spinner.set_option(option::PrefixText{"✔"});
   } else {
      spinner.mark_as_completed();
      spinner.set_option(option::ForegroundColor{Color::red});
      spinner.set_option(option::PrefixText{"⨯"});
      return_code = EXIT_FAILURE;
   }

   spinner.set_option(option::ShowSpinner{false});
   spinner.set_option(option::ShowPercentage{false});
   progress_message(final_text);

   return return_code;
}