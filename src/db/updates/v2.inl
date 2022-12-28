//
// Created by Dennis Sitelew on 28.12.22.
//

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

namespace {

void update_v2(db_t &db) {
   std::int64_t current_max = 0;

   // Get the maximal frame number
   auto max_stmt = std::make_unique<db::statement>("get_max_frame_num");
   max_stmt->prepare(db, R"sql(SELECT MAX(frame_num) FROM ocr_entries;)sql", false);
   max_stmt->reset();

   auto code = max_stmt->evaluate();
   if (code == SQLITE_ROW && !max_stmt->is_null(0)) {
      current_max = max_stmt->get_int64(0);
   }

   // Add the frame number column
   auto sql = R"sql(
BEGIN TRANSACTION;

ALTER TABLE metadata
ADD COLUMN last_processed_frame INT DEFAULT(0);

COMMIT;
)sql";
   ocs::db::statement::exec(db, sql);

   // Update the frame number itself
   std::string update = "UPDATE metadata SET last_processed_frame=";
   update += std::to_string(current_max);
   update += ';';

   ocs::db::statement::exec(db, update.c_str());
}

} // namespace