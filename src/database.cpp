//
// Created by Dennis Sitelew on 21.12.22.
//

#include <ocs/database.h>

#include <spdlog/spdlog.h>

using namespace ocs;
using namespace ocs::recognition;

using db_t = ocs::db::database;

// Include updates implementations. Those functions are usually very big and not that interesting, so they are
// implemented in standalone modules
#define OCS_IDL_INCLUDE
#include "db/updates/v0.inl"
#include "db/updates/v1.inl"
#include "db/updates/v2.inl"

// Note: should always be last
#include "db/updates/update.inl"
#undef OCS_IDL_INCLUDE

database::database(std::string db_path, bool read_only)
   : db_path_{std::move(db_path)}
   , db_(db_path_, read_only) {
   db_.init(CURRENT_DB_VERSION, &database::db_update);
   prepare_statements();
}

void database::store(const ocr::ocr_result &result) {
   store_last_frame_number(result.frame_number);
   if (result.entries.empty()) {
      return;
   }

   std::lock_guard lock{database_mutex_};

   if (is_frame_processed(result.frame_number)) {
      return;
   }

   auto &stmt = add_text_entry_;

   try {
      db::statement::exec(db_, "BEGIN TRANSACTION");
      for (auto &entry : result.entries) {
         stmt->reset();
         stmt->bind("pnum", result.frame_number);
         stmt->bind("pleft", entry.left);
         stmt->bind("ptop", entry.top);
         stmt->bind("pright", entry.right);
         stmt->bind("pbottom", entry.bottom);
         stmt->bind("pconfidence", entry.confidence);
         stmt->bind("ptext", entry.text);
         stmt->evaluate();
      }
      db::statement::exec(db_, "COMMIT TRANSACTION");
   } catch (const std::exception &e) {
      SPDLOG_ERROR("Failed to store OCR result for frame {}, {}", result.frame_number, e.what());
      db::statement::exec(db_, "ROLLBACK TRANSACTION");
      throw;
   } catch (...) {
      db::statement::exec(db_, "ROLLBACK TRANSACTION");
      SPDLOG_ERROR("Failed to store OCR result for frame {}", result.frame_number);
      throw;
   }
}

std::int64_t database::get_starting_frame_number() const {
   std::lock_guard lock{database_mutex_};
   auto &stmt = get_starting_frame_number_;

   stmt->reset();
   stmt->evaluate();

   return stmt->get_int64(0) + 1;
}

bool database::is_frame_processed(std::int64_t frame_num) const {
   std::lock_guard lock{database_mutex_};
   auto &stmt = is_frame_number_present_;

   stmt->reset();
   stmt->bind("pnum", frame_num);
   stmt->evaluate();

   return stmt->get_int(0) != 0;
}

void database::store_last_frame_number(std::int64_t frame_num) const {
   std::lock_guard lock{database_mutex_};

   static std::int64_t max_frame_number = -1;
   if (frame_num <= max_frame_number) {
      return;
   }

   auto &stmt = store_last_frame_number_;

   stmt->reset();
   stmt->bind("pnum", frame_num);
   stmt->evaluate();

   max_frame_number = frame_num;
}

void database::find_text(const std::string &text, std::vector<search_entry> &entries) const {
   entries.clear();

   auto &stmt = find_text_;
   stmt->reset();
   stmt->bind("ptext", text);
   auto code = stmt->evaluate();

   while (code == SQLITE_ROW) {
      entries.emplace_back();
      auto &entry = entries.back();
      entry.frame_number = stmt->get_int(0);
      entry.left = stmt->get_int(1);
      entry.top = stmt->get_int(2);
      entry.right = stmt->get_int(3);
      entry.bottom = stmt->get_int(4);
      entry.confidence = stmt->get_float(5);
      entry.text = stmt->get_text(6);

      code = stmt->evaluate();
   }
}

void database::prepare_statements() {
   // clang-format off
   get_starting_frame_number_ = std::make_unique<db::statement>("get_starting_frame_number");
   get_starting_frame_number_->prepare(db_, R"sql(SELECT last_processed_frame FROM metadata;)sql", true);

   add_text_entry_ = std::make_unique<db::statement>("add_text_entry");
   add_text_entry_->prepare(db_,
   R"sql(INSERT INTO ocr_entries("frame_num", "left", "top", "right", "bottom", "confidence", "ocr_text")
         VALUES (:pnum, :pleft, :ptop, :pright, :pbottom, :pconfidence, :ptext);)sql",
                           true);

   is_frame_number_present_ = std::make_unique<db::statement>("is_frame_number_present");
   is_frame_number_present_->prepare(db_,
   R"sql(SELECT COUNT(*) FROM ocr_entries WHERE frame_num = :pnum;)sql", true);

   store_last_frame_number_ = std::make_unique<db::statement>("store_last_frame_number");
   store_last_frame_number_->prepare(db_,
   R"sql(UPDATE metadata SET last_processed_frame=:pnum;)sql", true);

   find_text_ = std::make_unique<db::statement>("find_text");
   find_text_->prepare(db_,
   R"sql(SELECT frame_num, left, top, right, bottom, confidence, ocr_text FROM ocr_entries WHERE ocr_text LIKE :ptext;)sql", true);

   // clang-format on
}
