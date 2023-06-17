//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/viewer/results.h>
#include <ocs/viewer/views/frame_view.h>
#include <ocs/viewer/views/search_results_view.h>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cinttypes>

using namespace ocs::viewer::views;

search_results_view::search_results_view(search &srch, frame_view &frame_view)
   : search_{&srch}
   , frame_view_{&frame_view} {
   // Nothing to do here
}

void search_results_view::sort_results() {
   spdlog::debug("Sorting results...");
   days_.clear();

   day *current_day = nullptr;
   hour *current_hour = nullptr;
   minute *current_minute = nullptr;
   frame *current_frame = nullptr;

   results &res = search_->get_results();
   res.start_selection();

   auto date_time_from_video_file_start = [](const auto ts) {
      using namespace boost::gregorian;
      using namespace boost::local_time;
      using namespace boost::posix_time;

      const ptime time_t_epoch(date(1970, 1, 1));
      const auto as_posix = time_t_epoch + boost::posix_time::milliseconds(ts.count());
      return as_posix;
   };

   auto date_to_string = [](auto date) {
      using namespace boost::gregorian;
      using namespace boost::local_time;
      using namespace boost::posix_time;

      const auto date_format = "%Y-%m-%d";
      auto time_output_facet = new time_facet(date_format);

      std::stringstream ss;
      ss.imbue(std::locale(ss.getloc(), time_output_facet));

      ss << date;
      return ss.str();
   };

   auto update_frame_text = [&](auto &frame) {
      if (frame) {
         // Add number of entries as part of the text
         frame->name = fmt::format("{} - {}", frame->name, frame->texts.size());
      }
   };

   // NOTE: We cannot set the owners immediately, because the pointers may be invalidated once we add more siblings to
   // the parent

   using namespace std::chrono;
   results::optional_entry_t opt_entry = res.get_next();
   while (opt_entry) {
      const auto &entry = opt_entry.value();
      const auto file_start = results::start_time_for_video(entry.video_file);
      const auto entry_date_time = date_time_from_video_file_start(file_start);
      const auto entry_date = entry_date_time.date();
      const auto day_number = entry_date.julian_day();
      const auto frame_number = entry.frame;

      if (!current_day || current_day->number != day_number) {
         days_.push_back({});
         current_day = &days_.back();
         current_day->name = date_to_string(entry_date);
         current_day->number = day_number;
         current_day->owner_idx = days_.size() - 1;
         current_hour = nullptr;
      }

      if (!current_hour || current_hour->number != entry.hour) {
         current_day->hours.push_back({});
         current_hour = &current_day->hours.back();
         current_hour->number = entry.hour;
         current_hour->name = fmt::format("{:02}:??", entry.hour);
         current_hour->owner_idx = current_day->hours.size() - 1;
         current_minute = nullptr;
      }

      if (!current_minute || current_minute->number != entry.minute) {
         current_hour->minutes.push_back({});
         current_minute = &current_hour->minutes.back();
         current_minute->number = entry.minute;
         current_minute->name = fmt::format("{:02}", entry.minute);
         current_minute->owner_idx = current_hour->minutes.size() - 1;

         update_frame_text(current_frame);
         current_frame = nullptr;
      }

      if (!current_frame || current_frame->number != frame_number) {
         update_frame_text(current_frame);

         current_minute->frames.push_back({});
         current_frame = &current_minute->frames.back();
         current_frame->number = frame_number;
         current_frame->name = std::to_string(frame_number);
         current_frame->timestamp = entry.timestamp;
         current_frame->video_file = entry.video_file;
         current_frame->owner_idx = current_minute->frames.size() - 1;
      }

      text text_entry;
      text_entry.left = entry.left;
      text_entry.top = entry.top;
      text_entry.right = entry.right;
      text_entry.bottom = entry.bottom;
      text_entry.confidence = entry.confidence;
      text_entry.value = entry.text;
      text_entry.owner = current_frame;
      text_entry.owner_idx = current_frame->texts.size();

      current_frame->texts.emplace_back(std::move(text_entry));

      opt_entry = res.get_next();
   }

   // Reassign the owners
   for (auto &day : days_) {
      for (auto &hour : day.hours) {
         hour.owner = &day;

         for (auto &minute : hour.minutes) {
            minute.owner = &hour;

            for (auto &frame : minute.frames) {
               frame.owner = &minute;
            }
         }
      }
   }

   // Handle last frame entry
   update_frame_text(current_frame);

   spdlog::debug("Results sorted!");
}

void search_results_view::draw() {
   ImGui::Begin(name());

   const bool is_finished = search_->is_finished();
   if (is_finished && !is_finished_) {
      // Finished after starting a search on some other frame
      is_finished_ = true;
      sort_results();
   } else if (!is_finished && is_finished_) {
      // Started a new search
      is_finished_ = false;
   }

   for (const auto &day : days_) {
      if (ImGui::TreeNode(day.name.c_str())) {
         for (const auto &hour : day.hours) {
            if (ImGui::TreeNode(hour.name.c_str())) {
               for (const auto &minute : hour.minutes) {
                  if (ImGui::TreeNode(minute.name.c_str())) {
                     for (const auto &frame : minute.frames) {
                        if (ImGui::Button(frame.name.c_str())) {
                           frame_view_->set_current_frame(frame);
                        }

                        if (ImGui::IsItemHovered()) {
                           ImGui::SetTooltip("%s %.02d:%02d (%" PRIi64 ")", day.name.c_str(),
                                             static_cast<int>(hour.number), static_cast<int>(minute.number),
                                             frame.timestamp);
                        }
                     }
                     ImGui::TreePop();
                  }
               }
               ImGui::TreePop();
            }
         }
         ImGui::TreePop();
      }
   }

   ImGui::End();
}
