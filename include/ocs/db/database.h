//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCR_SUITE_DB_DATABASE_H
#define OCR_SUITE_DB_DATABASE_H

#include <functional>
#include <memory>
#include <string>
#include <string_view>

struct sqlite3;

namespace ocs::db {

class statement;

class database {
   friend class statement;

public:
   //! Database update function type
   //! Performs a single update step
   //! The `from` parameter denotes the current database version
   using update_funct_t = std::function<void(database &db, int from)>;

public:
   database(std::string path, bool read_only = false);
   ~database();

public:
   void init(int current_version,
             const update_funct_t &update_func,
             std::string_view version_table = "metadata",
             std::string_view version_column = "version");

   std::int64_t last_insert_rowid();

private:
   int get_file_version(std::string_view table, std::string_view column);
   void perform_update(int from, int to, const update_funct_t &func, const char *table, const char *column);

private:
   //! Path to the database
   const std::string path_;

   //! Is the database connection read-only?
   const bool read_only_;

   //! Database connection
   struct sqlite3 *db_;

   //! Update database version statement
   std::unique_ptr<statement> update_db_;
};

} // namespace ocs::db

#endif // OCR_SUITE_DB_DATABASE_H
