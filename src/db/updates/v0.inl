//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

namespace {

void update_v0(db_t &db) {
   auto sql = R"sql(
BEGIN TRANSACTION;

CREATE TABLE metadata(version INT);
INSERT INTO  metadata(version) VALUES (0);

CREATE TABLE ocr_entries (
   id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
   "frame_num" INT NOT NULL,
   "left" INT,
   "top" INT,
   "right" INT,
   "bottom" INT,
   "confidence" FLOAT,
   "ocr_text" TEXT NOT NULL
);

COMMIT;
)sql";
   return ocs::db::statement::exec(db, sql);
}

} // namespace