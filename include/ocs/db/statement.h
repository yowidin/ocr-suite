//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCR_SUITE_STATEMENT_H
#define OCR_SUITE_STATEMENT_H

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <sqlite3.h>

namespace ocs::db {

class database;

class statement {
public:
   statement(std::string name);
   ~statement();

public:
   void prepare(database &db, std::string_view sql, bool persistent);

   // Index-based binds
   void bind(int index, const std::string &value);
   void bind(int index, std::string_view value);
   void bind(int index, std::int32_t value);
   void bind(int index, std::int64_t value);
   void bind(int index, double value);
   void bind(int index, const void *blob, std::int64_t size);

   // Name-based binds
   void bind(std::string_view param, const std::string &value);
   void bind(std::string_view param, std::string_view value);
   void bind(std::string_view param, std::int32_t value);
   void bind(std::string_view param, std::int64_t value);
   void bind(std::string_view param, double value);
   void bind(std::string_view param, const void *blob, std::int64_t size);

   // Getters
   std::int32_t get_int(int index);
   std::int64_t get_int64(int index);
   float get_float(int index);
   double get_double(int index);
   std::string get_text(int index);
   template <class Container>
   void get_blob(int index, Container &out);

   bool is_int(int index);
   bool is_float(int index);
   bool is_text(int index);
   bool is_blob(int index);
   bool is_null(int index);

   int evaluate(const std::initializer_list<int> &allowed_results = {SQLITE_OK, SQLITE_DONE, SQLITE_ROW});

   void reset();

   static void exec(database &db, const char *sql);

private:
   void fill_parameters_map();

private:
   //! Statement name
   std::string name_;

   //! Prepared statement
   sqlite3_stmt *stmt_;

   //! Named parameters map
   std::optional<std::map<std::string, int>> parameters_;
};

} // namespace ocs::db

#endif // OCR_SUITE_STATEMENT_H
