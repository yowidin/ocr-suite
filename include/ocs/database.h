//
// Created by Dennis Sitelew on 21.12.22.
//

#pragma once

#include <ocs/recognition/ocr.h>

#include <ocs/db/database.h>
#include <ocs/db/statement.h>

#include <memory>
#include <mutex>

struct sqlite3;

namespace ocs {

//! A class for storing the results of the OCR process in a sqlite3 database.
class database {
private:
   static const int CURRENT_DB_VERSION;

   //! Unique statement pointer type
   using statement_ptr_t = std::unique_ptr<db::statement>;

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
   void store(const ocs::recognition::ocr::ocr_result &result);

   std::int64_t get_starting_frame_number() const;
   bool is_frame_processed(std::int64_t frame_num) const;

   void store_last_frame_number(std::int64_t frame_num) const;

   void find_text(const std::string &text, std::vector<search_entry> &entries) const;

private:
   static void db_update(db::database &db, int from);

   void prepare_statements();

private:
   std::string db_path_;
   ocs::db::database db_;

   statement_ptr_t add_text_entry_;
   statement_ptr_t get_starting_frame_number_;
   statement_ptr_t is_frame_number_present_;
   statement_ptr_t store_last_frame_number_;
   statement_ptr_t find_text_;

   mutable std::recursive_mutex database_mutex_{};
};

} // namespace ocs
