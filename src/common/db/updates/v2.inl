//
// Created by Dennis Sitelew on 28.12.22.
//

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

namespace {

void update_v2(sqlite_burrito::versioned_database &db) {
   std::int64_t current_max = 0;

   // Get the maximal frame number
   sqlite_burrito::statement max_stmt{db.get_connection()};
   max_stmt.prepare(R"sql(SELECT MAX(frame_num) FROM ocr_entries;)sql");
   max_stmt.reset();

   auto have_value = max_stmt.step();
   if (have_value && !max_stmt.is_null(0)) {
      max_stmt.get(0, current_max);
   }

   // Add the frame number column
   auto sql = R"sql(
BEGIN TRANSACTION;

ALTER TABLE metadata
ADD COLUMN last_processed_frame INT DEFAULT(0);

COMMIT;
)sql";
   sqlite_burrito::statement::execute(db.get_connection(), sql);

   // Update the frame number itself
   std::string update = "UPDATE metadata SET last_processed_frame=";
   update += std::to_string(current_max);
   update += ';';

   sqlite_burrito::statement::execute(db.get_connection(), update);
}

} // namespace