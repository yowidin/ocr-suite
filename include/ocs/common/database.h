//
// Created by Dennis Sitelew on 21.12.22.
//

#pragma once

#include <ocs/common/ocr_result.h>

#include <sqlite-burrito/versioned_database.h>

#include <memory>
#include <mutex>

struct sqlite3;

namespace ocs::common {

//! A class for storing the results of the OCR process in a sqlite3 database.
class database {
private:
   static const int CURRENT_DB_VERSION;

   using statement_t = sqlite_burrito::statement;

public:
   struct search_entry {
      int frame_number;
      std::string text;
      int left, top, right, bottom;
      float confidence;
   };

public:
   database(std::string db_path, bool read_only = false);

public:
   void store(const ocr_result &result);

   std::int64_t get_starting_frame_number();
   bool is_frame_processed(std::int64_t frame_num);

   void store_last_frame_number(std::int64_t frame_num);

   void find_text(const std::string &text, std::vector<search_entry> &entries);

private:
   static void db_update(sqlite_burrito::versioned_database &con, int from, std::error_code &ec);

   void prepare_statements();

private:
   bool read_only_;
   std::string db_path_;
   sqlite_burrito::versioned_database db_;

   statement_t add_text_entry_;
   statement_t get_starting_frame_number_;
   statement_t is_frame_number_present_;
   statement_t store_last_frame_number_;
   statement_t find_text_;

   mutable std::recursive_mutex database_mutex_{};
};

} // namespace ocs
