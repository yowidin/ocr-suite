//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCR_SUITE_DATABASE_H
#define OCR_SUITE_DATABASE_H

#include <ocs/ocr.h>

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
   database(std::string db_path);

public:
   void store(const ocr::ocr_result &result);

   std::int64_t get_starting_frame_number() const;
   bool is_frame_processed(std::int64_t frame_num) const;

private:
   static void db_update(db::database &db, int from);

   void prepare_statements();

private:
   std::string db_path_;
   ocs::db::database db_;

   statement_ptr_t add_text_entry_;
   statement_ptr_t get_starting_frame_number_;
   statement_ptr_t is_frame_number_present_;

   mutable std::recursive_mutex database_mutex_{};
};

} // namespace ocs

#endif // OCR_SUITE_DATABASE_H
