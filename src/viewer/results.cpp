//
// Created by Dennis Sitelew on 06.03.23.
//

#include <ocs/common/video.h>
#include <ocs/viewer/results.h>

#include <spdlog/spdlog.h>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/filesystem.hpp>

// Include updates implementations. Those functions are usually very big and not that interesting, so they are
// implemented in standalone modules
#define OCS_VIEWER_IDL_INCLUDE
#include "db/updates/v0.inl"

// Note: should always be last
#include "db/updates/update.inl"
#undef OCS_VIEWER_IDL_INCLUDE

using namespace ocs::viewer;
namespace fs = boost::filesystem;

using open_flags_t = sqlite_burrito::connection::open_flags;
using flags_t = sqlite_burrito::statement::prepare_flags;

results::results(bool in_memory)
   : db_path_{in_memory ? ":memory:" : "search-results.db"}
   , db_{in_memory ? (open_flags_t::memory | open_flags_t::default_mode) : open_flags_t::default_mode}
   , add_text_entry_{db_.get_connection(), flags_t::persistent}
   , clear_{db_.get_connection(), flags_t::persistent}
   , select_{db_.get_connection(), flags_t::persistent} {
   db_.open(db_path_, CURRENT_DB_VERSION, &results::db_update);
   prepare_statements();
}

void results::store(const search::search_entry &result) {
   using namespace std::chrono;

   ocs::common::video video{result.video_file_path, ffmpeg::decoder::frame_filter::all_frames, {nullptr}};

   const auto start_time = start_time_for_video(result.video_file_path);

   auto &stmt = add_text_entry_;

   try {
      auto transaction = db_.get_connection().begin_transaction();

      for (auto &entry : result.entries) {
         const auto timestamp = start_time + video.frame_number_to_milliseconds(entry.frame_number);

         auto total_seconds = duration_cast<seconds>(timestamp - start_time);
         const auto num_hours = duration_cast<hours>(total_seconds);
         total_seconds -= num_hours;
         const auto num_minutes = duration_cast<minutes>(total_seconds);

         stmt.reset();
         stmt.bind(":pts", timestamp.count());
         stmt.bind(":pnum", entry.frame_number);
         stmt.bind(":pleft", entry.left);
         stmt.bind(":ptop", entry.top);
         stmt.bind(":pright", entry.right);
         stmt.bind(":pbottom", entry.bottom);
         stmt.bind(":pconfidence", entry.confidence);
         stmt.bind(":ptext", entry.text);
         stmt.bind(":pfile", result.video_file_path);
         stmt.bind(":phour", (int)num_hours.count());
         stmt.bind(":pminute", (int)num_minutes.count());
         stmt.execute();
      }

      transaction.commit();
   } catch (const std::exception &e) {
      spdlog::error("Failed to store search results for file {}, {}", result.video_file_path, e.what());
      throw;
   } catch (...) {
      spdlog::error("Failed to store search results for file {}", result.video_file_path);
      throw;
   }
}

void results::clear() {
   clear_.reset();
   clear_.execute();
}

void results::start_selection() {
   select_.reset();
}

results::optional_entry_t results::get_next() {
   auto &stmt = select_;

   if (!stmt.step()) {
      return {};
   }

   entry result;
   stmt.get(0, result.timestamp);
   stmt.get(1, result.frame);
   stmt.get(2, result.left);
   stmt.get(3, result.top);
   stmt.get(4, result.right);
   stmt.get(5, result.bottom);
   stmt.get(6, result.confidence);
   stmt.get(7, result.text);
   stmt.get(8, result.video_file);
   stmt.get(9, result.hour);
   stmt.get(10, result.minute);

   return result;
}

void results::prepare_statements() {
   add_text_entry_.prepare(
       R"sql(INSERT INTO results("timestamp", "frame_number", "left", "top", "right", "bottom", "confidence", "ocr_text", "video_file", "hour", "minute")
             VALUES (:pts, :pnum, :pleft, :ptop, :pright, :pbottom, :pconfidence, :ptext, :pfile, :phour, :pminute);)sql");

   clear_.prepare(R"sql(DELETE FROM results;)sql");

   select_.prepare(
       R"sql(SELECT timestamp, frame_number, "left", top, "right", bottom, confidence, ocr_text, video_file, hour, minute
                          FROM results
                          ORDER BY timestamp;)sql");
}

std::chrono::milliseconds results::start_time_for_video(const std::string &video_file) {
   using namespace boost::gregorian;
   using namespace boost::local_time;
   using namespace boost::posix_time;

   const auto base_name = fs::path(video_file).stem().string();

   const auto time_format = "%Y-%m-%d %H-%M-%S";
   auto time_input_facet = new local_time_input_facet(time_format);

   std::stringstream ss(base_name);
   ss.imbue(std::locale(ss.getloc(), time_input_facet));

   local_date_time start_time{not_a_date_time};
   ss >> start_time;

   const ptime time_t_epoch(date(1970, 1, 1));
   const time_duration diff = start_time.utc_time() - time_t_epoch;
   const auto start_ms = diff.total_milliseconds();

   return std::chrono::milliseconds{start_ms};
}
