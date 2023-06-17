//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_SEARCH_RESULTS_VIEW_H
#define OCR_SUITE_SEARCH_RESULTS_VIEW_H

#include <ocs/viewer/search.h>
#include <ocs/viewer/views/drawable.h>

#include <vector>

namespace ocs::viewer::views {

class frame_view;

class search_results_view : public drawable {
public:
   struct frame;
   struct minute;
   struct hour;
   struct day;

   struct text {
      int left, top, right, bottom;
      float confidence;
      std::string value;

      frame *owner;
      std::size_t owner_idx;
   };

   struct frame {
      std::string name;
      std::int64_t number;
      std::int64_t timestamp;
      std::string video_file;

      std::vector<text> texts;

      minute *owner;
      std::size_t owner_idx;
   };

   struct minute {
      std::string name;
      std::int64_t number;
      std::vector<frame> frames;

      hour *owner;
      std::size_t owner_idx;
   };

   struct hour {
      std::string name;
      std::int64_t number;
      std::vector<minute> minutes;

      day *owner;
      std::size_t owner_idx;
   };

   struct day {
      std::string name;
      std::int64_t number;
      std::vector<hour> hours;

      std::size_t owner_idx;
   };

public:
   search_results_view(search &srch, frame_view &frame_view);

public:
   void draw() override;
   const char *name() const override { return "SearchResults"; }

   auto &get_days() { return days_; }

private:
   void sort_results();

private:
   search *search_;
   frame_view *frame_view_;

   bool is_finished_{true};

   std::vector<day> days_{};
};

} // namespace ocs::viewer::views

#endif // OCR_SUITE_SEARCH_RESULTS_VIEW_H
