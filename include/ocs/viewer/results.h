//
// Created by Dennis Sitelew on 03.03.23.
//

#pragma once

#include <ocs/viewer/search.h>

#include <ocs/db/database.h>
#include <ocs/db/statement.h>

#include <memory>
#include <mutex>
#include <optional>

struct sqlite3;

namespace ocs::viewer {

//! A class for storing the results of the OCR process in a sqlite3 database.
class results {
private:
   static const int CURRENT_DB_VERSION;

   //! Unique statement pointer type
   using statement_ptr_t = std::unique_ptr<db::statement>;

public:
   struct entry {
      std::int64_t timestamp;
      std::int64_t frame;
      int left, top, right, bottom;
      float confidence;
      std::string text;
      std::string video_file;
      int hour, minute;
   };
   using optional_entry_t = std::optional<entry>;

public:
   results(bool in_memory);

public:
   void store(const search::search_entry &result);
   void clear();

   void start_selection();
   optional_entry_t get_next();

   static std::chrono::milliseconds start_time_for_video(const std::string &video_file);

private:
   static void db_update(db::database &db, int from);

   void prepare_statements();

private:
   std::string db_path_;
   ocs::db::database db_;

   statement_ptr_t add_text_entry_;
   statement_ptr_t clear_;
   statement_ptr_t select_;

   mutable std::recursive_mutex database_mutex_{};
};

} // namespace ocs
