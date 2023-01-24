//
// Created by Dennis Sitelew on 21.12.22.
//

#include <ocs/db/database.h>
#include <ocs/db/statement.h>

#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <chrono>
#include <memory>
#include <thread>

using namespace ocs::db;

namespace {

void errorLogCallback(void *pArg, int iErrCode, const char *zMsg) {
   SPDLOG_WARN("[{}] SQLite error: {}, ", iErrCode, zMsg);
}

struct error_log_setter {
   error_log_setter() { sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, nullptr); }
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
/// Class: statement
////////////////////////////////////////////////////////////////////////////////
database::database(std::string path, bool read_only)
   : path_{std::move(path)}
   , read_only_{read_only}
   , db_{nullptr}
   , update_db_{nullptr} {
   static error_log_setter setter;
}

database::~database() {
   if (!db_) {
      return;
   }

   SPDLOG_TRACE("Closing database connection: {}", path_);

   auto rc = sqlite3_close(db_);
   while (rc != SQLITE_OK) {
      SPDLOG_TRACE("[{}] Error closing the DB connection: {}", rc, path_);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      rc = sqlite3_close(db_);
   }
}

void database::init(int current_version,
                    const update_funct_t &update_func,
                    std::string_view version_table,
                    std::string_view version_column) {
   SPDLOG_TRACE("Initializing database: {}", path_);

   int code;
   if (read_only_) {
      code = sqlite3_open_v2(path_.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr);
   } else {
      code = sqlite3_open(path_.c_str(), &db_);
   }
   if (code) {
      throw std::runtime_error(fmt::format("Can't open database: {}", sqlite3_errmsg(db_)));
   }

   auto file_version = get_file_version(version_table, version_column);
   if (file_version < current_version) {
      perform_update(file_version, current_version, update_func, version_table.data(), version_column.data());
      update_db_.reset(nullptr);
   } else {
      SPDLOG_TRACE("Database is up to date: {}, version is {}", path_, file_version);
   }

   sqlite3_busy_timeout(db_, 10000);
}

std::int64_t database::last_insert_rowid() {
   return sqlite3_last_insert_rowid(db_);
}

int database::get_file_version(std::string_view table, std::string_view column) {
   statement meta_select{"get_version_table"};
   meta_select.prepare(*this, "SELECT name FROM sqlite_master WHERE type='table' AND name=:table;", false);

   meta_select.bind("table", table);
   auto rc = meta_select.evaluate({SQLITE_ROW, SQLITE_DONE});
   if (rc != SQLITE_ROW) {
      // No such table
      return 0;
   }

   // Construct version select statement
   std::string sql = fmt::format(R"(SELECT "{}" FROM "{}";)", column, table);

   statement version{"get_version_value"};
   version.prepare(*this, sql, false);
   version.evaluate({SQLITE_ROW});
   return version.get_int(0);
}

void database::perform_update(int from, int to, const update_funct_t &func, const char *table, const char *column) {
   if (from >= to) {
      return;
   }

   int current{from + 1};
   SPDLOG_TRACE("Updating database: {}, from version {} to {}", path_, from, to);
   func(*this, from);

   // Write new version
   if (!update_db_) {
      auto msg = fmt::format(R"(UPDATE "{}" SET "{}" = :version;)", table, column);
      update_db_ = std::make_unique<statement>("update_db");
      update_db_->prepare(*this, msg, false);
   } else {
      update_db_->reset();
   }

   update_db_->bind(1, current);
   perform_update(current, to, func, table, column);
   update_db_->evaluate();
}
