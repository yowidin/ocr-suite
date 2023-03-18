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

   auto date_time_from_timestamp = [](const auto ts) {
      using namespace boost::gregorian;
      using namespace boost::local_time;
      using namespace boost::posix_time;

      const ptime time_t_epoch(date(1970, 1, 1));
      const auto as_posix = time_t_epoch + milliseconds(ts);

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

   results::optional_entry_t opt_entry = res.get_next();
   while (opt_entry) {
      const auto &entry = opt_entry.value();
      const auto entry_date_time = date_time_from_timestamp(entry.timestamp);
      const auto entry_date = entry_date_time.date();
      const auto entry_time = entry_date_time.time_of_day();
      const auto day_number = entry_date.julian_day();
      const auto hour_number = entry_time.hours();
      const auto minute_number = entry_time.minutes();
      const auto frame_number = entry.frame;

      if (!current_day || current_day->number != day_number) {
         days_.push_back({});
         current_day = &days_.back();
         current_day->name = date_to_string(entry_date);
         current_day->number = day_number;
      }

      if (!current_hour || current_hour->number != hour_number) {
         current_day->hours.push_back({});
         current_hour = &current_day->hours.back();
         current_hour->number = hour_number;
         current_hour->name = fmt::format("{:02}:??", hour_number);
         current_hour->owner = current_day;
      }

      if (!current_minute || current_minute->number != minute_number) {
         current_hour->minutes.push_back({});
         current_minute = &current_hour->minutes.back();
         current_minute->number = minute_number;
         current_minute->name = fmt::format("{:02}", minute_number);
         current_minute->owner = current_hour;
      }

      if (!current_frame || current_frame->number != frame_number) {
         update_frame_text(current_frame);

         current_minute->frames.push_back({});
         current_frame = &current_minute->frames.back();
         current_frame->number = frame_number;
         current_frame->name = std::to_string(frame_number);
         current_frame->timestamp = entry.timestamp;
         current_frame->video_file = entry.video_file;
         current_frame->owner = current_minute;
      }

      text text_entry = {
          .left = entry.left,
          .top = entry.top,
          .right = entry.right,
          .bottom = entry.bottom,
          .confidence = entry.confidence,
          .text = entry.text,
          .owner = current_frame,
      };

      current_frame->texts.emplace_back(std::move(text_entry));

      opt_entry = res.get_next();
   }

   // Handle last frame entry
   update_frame_text(current_frame);

   spdlog::debug("Results sorted!");
}

void search_results_view::draw() {
   ImGui::Begin(name());

   static const char *text = "Search Results....";

   const bool is_finished = search_->is_finished();
   if (is_finished && !is_finished_) {
      // Finished after starting a search on some other frame
      text = "Search done!";
      is_finished_ = true;
      sort_results();
   } else if (!is_finished && is_finished_) {
      // Started a new search
      text = "Search started!";
      is_finished_ = false;
   }

   ImGui::Text("%s", text);

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
                           ImGui::SetTooltip("%s %.02d:%02d", day.name.c_str(), static_cast<int>(hour.number),
                                             static_cast<int>(minute.number));
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
