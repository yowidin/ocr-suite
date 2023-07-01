//
// Created by Dennis Sitelew on 29.06.23.
//

#include <indicators/progress_spinner.hpp>

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

namespace {

void update_v3_safe(sqlite_burrito::versioned_database &db) {
   auto schema_update = R"sql(
CREATE TABLE text_entries (
   id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
   "value" TEXT UNIQUE NOT NULL
);

CREATE INDEX text_entries_value_idx ON text_entries(value);

CREATE TABLE text_instances (
   id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
   text_entry_id INTEGER NOT NULL,
   "frame_num" INT NOT NULL,
   "left" INT,
   "top" INT,
   "right" INT,
   "bottom" INT,
   "confidence" FLOAT,

   FOREIGN KEY(text_entry_id)
      REFERENCES text_entries(id)
      ON UPDATE CASCADE
      ON DELETE CASCADE
);

CREATE INDEX text_instances_frame_num_idx ON text_instances(frame_num);
)sql";
   sqlite_burrito::statement::execute(db.get_connection(), schema_update);

   sqlite_burrito::statement sel_stmt{db.get_connection()};
   sel_stmt.prepare(R"sql(SELECT frame_num, left, top, right, bottom, confidence, ocr_text FROM ocr_entries;)sql");

   sqlite_burrito::statement ins_text_stmt{db.get_connection()};
   ins_text_stmt.prepare(R"sql(INSERT OR IGNORE INTO text_entries("value") VALUES (:pvalue);)sql");

   sqlite_burrito::statement ins_ocr_stmt{db.get_connection()};
   ins_ocr_stmt.prepare(
       R"sql(INSERT INTO text_instances("text_entry_id", "frame_num", "left", "top", "right", "bottom", "confidence")
             VALUES (:ptid, :pnum, :pleft, :ptop, :pright, :pbottom, :pconfidence);)sql");

   sqlite_burrito::statement get_tid_stmt{db.get_connection()};
   get_tid_stmt.prepare(R"sql(SELECT id FROM text_entries WHERE value == :ptext;)sql");

   sqlite_burrito::statement get_num_ocr_entries{db.get_connection()};
   get_num_ocr_entries.prepare(R"sql(SELECT COUNT(id) FROM ocr_entries;)sql");

   std::int64_t total_entries = 0;
   get_num_ocr_entries.reset();
   get_num_ocr_entries.step();
   get_num_ocr_entries.get(0, total_entries);

   using namespace indicators;
   indicators::ProgressSpinner spinner{
       option::PostfixText{"migrating..."},
       option::ShowPercentage{true},
       option::ShowElapsedTime{true},
       option::ShowRemainingTime{true},
       option::ShowSpinner{false},
   };
   spinner.set_option(option::MaxProgress{total_entries});

   auto add_text_entry = [&](const std::string &text) {
      ins_text_stmt.reset();
      ins_text_stmt.bind(":pvalue", text);
      ins_text_stmt.execute();
   };

   auto get_text_entry_id = [&](const std::string &text) {
      get_tid_stmt.reset();
      get_tid_stmt.bind(":ptext", text);
      get_tid_stmt.step();

      std::int64_t result;
      get_tid_stmt.get(0, result);
      return result;
   };

   auto migrate_one_entry = [&](const database::search_entry &e) {
      add_text_entry(e.text);
      auto text_id = get_text_entry_id(e.text);

      ins_ocr_stmt.reset();
      ins_ocr_stmt.bind(":ptid", text_id);
      ins_ocr_stmt.bind(":pnum", e.frame_number);
      ins_ocr_stmt.bind(":pleft", e.left);
      ins_ocr_stmt.bind(":ptop", e.top);
      ins_ocr_stmt.bind(":pright", e.right);
      ins_ocr_stmt.bind(":pbottom", e.bottom);
      ins_ocr_stmt.bind(":pconfidence", e.confidence);

      ins_ocr_stmt.execute();
   };

   std::int64_t num_processed = 0;

   spdlog::info("Copying entries to the new tables");

   sel_stmt.reset();
   while (sel_stmt.step()) {
      database::search_entry e;
      sel_stmt.get(0, e.frame_number);
      sel_stmt.get(1, e.left);
      sel_stmt.get(2, e.top);
      sel_stmt.get(3, e.right);
      sel_stmt.get(4, e.bottom);
      sel_stmt.get(5, e.confidence);
      sel_stmt.get(6, e.text);
      migrate_one_entry(e);

      ++num_processed;
      spinner.set_progress(num_processed);
   }
}

void update_v3(sqlite_burrito::versioned_database &db) {
   // Migrate to a more compact layout

   try {
      spdlog::info("Updating DB schema");

      auto transaction = db.get_connection().begin_transaction();

      update_v3_safe(db);

      transaction.commit();

      spdlog::info("Dropping old table");
      sqlite_burrito::statement::execute(db.get_connection(), "DROP TABLE ocr_entries;");

      spdlog::info("Cleaning up");
      sqlite_burrito::statement::execute(db.get_connection(), "VACUUM;");

      spdlog::info("Migration done!");
   } catch (const std::exception &e) {
      spdlog::error("Database upgrade failed: {}", e.what());
      throw;
   } catch (...) {
      spdlog::error("Database upgrade failed");
      throw;
   }
}

} // namespace