//
// Created by Dennis Sitelew on 24.12.22.
//

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

namespace {

void update_v1(db_t &db) {
   auto sql = R"sql(
BEGIN TRANSACTION;

CREATE INDEX frame_numbers_idx ON ocr_entries(frame_num);
CREATE INDEX frame_text_idx ON ocr_entries(ocr_text);

COMMIT;
)sql";
   return ocs::db::statement::exec(db, sql);
}

} // namespace