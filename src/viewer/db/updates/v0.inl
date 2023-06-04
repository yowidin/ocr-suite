//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCS_VIEWER_IDL_INCLUDE
#error Internal use only
#endif

namespace {

void update_v0(sqlite_burrito::versioned_database &db) {
   auto sql = R"sql(
BEGIN TRANSACTION;

CREATE TABLE metadata(version INT);
INSERT INTO  metadata(version) VALUES (0);

CREATE TABLE results (
   id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
   "timestamp" INT NOT NULL,
   "frame_number" INT NOT NULL,
   "left" INT,
   "top" INT,
   "right" INT,
   "bottom" INT,
   "confidence" FLOAT,
   "ocr_text" TEXT NOT NULL,
   "video_file" TEXT NOT NULL,
   "hour" INT,
   "minute" INT
);

CREATE INDEX timestamp_idx ON results(timestamp);

COMMIT;
)sql";
   sqlite_burrito::statement::execute(db.get_connection(), sql);
}

} // namespace