//
// Created by Dennis Sitelew on 21.12.22.
//

#include <ocs/common/database.h>

#include <spdlog/spdlog.h>

using namespace ocs::common;

// Include updates implementations. Those functions are usually very big and not that interesting, so they are
// implemented in standalone modules
#define OCS_IDL_INCLUDE
#include "db/updates/v0.inl"
#include "db/updates/v1.inl"
#include "db/updates/v2.inl"
#include "db/updates/v3.inl"

// Note: should always be last
#include "db/updates/update.inl"
#undef OCS_IDL_INCLUDE

using open_flags_t = sqlite_burrito::connection::open_flags;
using flags_t = sqlite_burrito::statement::prepare_flags;

namespace {

void sqlite3_error_callback(void *pArg, int iErrCode, const char *zMsg) {
   spdlog::error("SQLite3 error: {} [{}]", zMsg, iErrCode);
}

} // namespace

database::database(std::string db_path, bool read_only)
   : read_only_{read_only}
   , db_path_{std::move(db_path)}
   , db_{read_only ? open_flags_t::readonly : open_flags_t::default_mode}
   , add_text_entry_{db_.get_connection(), flags_t::persistent}
   , get_text_entry_id_{db_.get_connection(), flags_t::persistent}
   , add_text_instance_{db_.get_connection(), flags_t::persistent}
   , get_starting_frame_number_{db_.get_connection(), flags_t::persistent}
   , is_frame_number_present_{db_.get_connection(), flags_t::persistent}
   , store_last_frame_number_{db_.get_connection(), flags_t::persistent}
   , find_text_{db_.get_connection(), flags_t::persistent} {
   sqlite3_config(SQLITE_CONFIG_LOG, sqlite3_error_callback, nullptr);
   db_.open(db_path_, CURRENT_DB_VERSION, &database::db_update);
   prepare_statements();
}

void database::store(const common::ocr_result &result) {
   store_last_frame_number(result.frame_number);
   if (result.entries.empty()) {
      return;
   }

   std::lock_guard lock{database_mutex_};

   if (is_frame_processed(result.frame_number)) {
      return;
   }

   auto add_text_entry = [&](const std::string &text) {
      auto &stmt = add_text_entry_;
      stmt.reset();
      stmt.bind(":pvalue", text);
      stmt.execute();
   };

   auto get_text_entry_id = [&](const std::string &text) {
      auto &stmt = get_text_entry_id_;
      stmt.reset();
      stmt.bind(":ptext", text);
      stmt.step();

      std::int64_t result;
      stmt.get(0, result);
      return result;
   };

   auto &stmt = add_text_instance_;

   try {
      auto transaction = db_.get_connection().begin_transaction();

      for (auto &entry : result.entries) {
         add_text_entry(entry.text);
         auto text_id = get_text_entry_id(entry.text);

         stmt.reset();
         stmt.bind(":ptid", text_id);
         stmt.bind(":pnum", result.frame_number);
         stmt.bind(":pleft", entry.left);
         stmt.bind(":ptop", entry.top);
         stmt.bind(":pright", entry.right);
         stmt.bind(":pbottom", entry.bottom);
         stmt.bind(":pconfidence", entry.confidence);
         stmt.execute();
      }

      transaction.commit();
   } catch (const std::exception &e) {
      spdlog::error("Failed to store OCR result for frame {}, {}", result.frame_number, e.what());
      throw;
   } catch (...) {
      spdlog::error("Failed to store OCR result for frame {}", result.frame_number);
      throw;
   }
}

std::int64_t database::get_starting_frame_number() {
   std::lock_guard lock{database_mutex_};
   auto &stmt = get_starting_frame_number_;

   stmt.reset();
   stmt.step();

   std::int64_t result;
   stmt.get(0, result);
   return result + 1;
}

bool database::is_frame_processed(std::int64_t frame_num) {
   std::lock_guard lock{database_mutex_};
   auto &stmt = is_frame_number_present_;

   stmt.reset();
   stmt.bind(":pnum", frame_num);
   stmt.step();

   bool result;
   stmt.get(0, result);
   return result;
}

void database::store_last_frame_number(std::int64_t frame_num) {
   std::lock_guard lock{database_mutex_};

   static std::int64_t max_frame_number = -1;
   if (frame_num <= max_frame_number) {
      return;
   }

   auto &stmt = store_last_frame_number_;

   stmt.reset();
   stmt.bind(":pnum", frame_num);
   stmt.execute();

   max_frame_number = frame_num;
}

void database::find_text(const std::string &text, std::vector<search_entry> &entries) {
   entries.clear();

   auto &stmt = find_text_;
   stmt.reset();
   stmt.bind(":ptext", text);

   while (stmt.step()) {
      entries.emplace_back();
      auto &e = entries.back();
      stmt.get(0, e.frame_number);
      stmt.get(1, e.left);
      stmt.get(2, e.top);
      stmt.get(3, e.right);
      stmt.get(4, e.bottom);
      stmt.get(5, e.confidence);
      stmt.get(6, e.text);
   }
}

void database::prepare_statements() {
   // clang-format off
   get_starting_frame_number_.prepare(R"sql(SELECT last_processed_frame FROM metadata;)sql");

   if (!read_only_) {
      add_text_instance_.prepare(
R"sql(INSERT INTO text_instances("text_entry_id", "frame_num", "left", "top", "right", "bottom", "confidence")
      VALUES (:ptid, :pnum, :pleft, :ptop, :pright, :pbottom, :pconfidence);)sql");

      add_text_entry_.prepare(R"sql(INSERT OR IGNORE INTO text_entries("value") VALUES (:pvalue);)sql");

      store_last_frame_number_.prepare(R"sql(UPDATE metadata SET last_processed_frame=:pnum;)sql");
   }

   get_text_entry_id_.prepare(R"sql(SELECT id FROM text_entries WHERE value == :ptext;)sql");

   is_frame_number_present_.prepare(R"sql(SELECT COUNT(*) FROM text_instances WHERE frame_num = :pnum;)sql");

   find_text_.prepare(
R"sql(SELECT frame_num, "left", top, "right", bottom, confidence, value
      FROM text_instances
      LEFT JOIN  text_entries te ON text_instances.text_entry_id = te.id
      WHERE te.value LIKE :ptext;)sql");

   // clang-format on
}
