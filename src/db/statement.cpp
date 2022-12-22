//
// Created by Dennis Sitelew on 21.12.22.
//

#include <ocs/db/database.h>
#include <ocs/db/statement.h>

#include <spdlog/spdlog.h>

using ocs::db::statement;

////////////////////////////////////////////////////////////////////////////////
/// Class: statement
////////////////////////////////////////////////////////////////////////////////
statement::statement(std::string name)
   : name_{std::move(name)}
   , stmt_{nullptr}
   , parameters_{} {
   // Nothing to do here
}

/**
 * Deconstruct the statement
 */
statement::~statement() {
   if (!stmt_) {
      return;
   }

   sqlite3_finalize(stmt_);
}

/**
 * Prepare a statement
 * @param db Database object
 * @param sql SQL Statement
 */
void statement::prepare(database &db, std::string_view sql, bool persistent) {
   auto rc = sqlite3_prepare_v3(db.db_, sql.data(), static_cast<int>(sql.size()),
                                (persistent ? SQLITE_PREPARE_PERSISTENT : 0), &stmt_, nullptr);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{
          fmt::format("[{}] Failed to prepare statement: {} {}", rc, name_, sqlite3_errmsg(db.db_))};
   }
}

/**
 * Fill up the statement parameters map
 */
void statement::fill_parameters_map() {
   if (parameters_) {
      // Already filled up
      return;
   }

   // Construct default map
   parameters_ = decltype(parameters_)::value_type{};
   auto &map = parameters_.value();

   auto max_index = sqlite3_bind_parameter_count(stmt_);
   for (int i = 1; i <= max_index; ++i) {
      auto ptr = sqlite3_bind_parameter_name(stmt_, i);
      if (ptr != nullptr) {
         map[std::string(&ptr[1])] = i;
      }
   }
}

/**
 * Bind string parameter
 * @param param Parameter index (starts with 1)
 * @param value Parameter value
 */
void statement::bind(int index, const std::string &value) {
   auto rc = sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{fmt::format("[{}] Failed to bind parameter: {} in {}", rc, index, name_)};
   }
}

void statement::bind(int index, std::string_view value) {
   auto rc = sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{fmt::format("[{}] Failed to bind parameter: {} in {}", rc, index, name_)};
   }
}

/**
 * Bind 32-bit integer parameter
 * @param param Parameter index (starts with 1)
 * @param value Parameter value
 */
void statement::bind(int index, std::int32_t value) {
   auto rc = sqlite3_bind_int(stmt_, index, value);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{fmt::format("[{}] Failed to bind parameter: {} in {}", rc, index, name_)};
   }
}

/**
 * Bind 64-bit integer parameter
 * @param param Parameter index (starts with 1)
 * @param value Parameter value
 */
void statement::bind(int index, std::int64_t value) {
   auto rc = sqlite3_bind_int64(stmt_, index, value);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{fmt::format("[{}] Failed to bind parameter: {} in {}", rc, index, name_)};
   }
}

/**
 * Bind a double parameter
 * @param param Parameter index (starts with 1)
 * @param value Parameter value
 */
void statement::bind(int index, double value) {
   auto rc = sqlite3_bind_double(stmt_, index, value);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{fmt::format("[{}] Failed to bind parameter: {} in {}", rc, index, name_)};
   }
}

/**
 * Bind BLOB parameter
 * @param param Parameter index (starts with 1)
 * @param blob Parameter value
 * @param size Parameter size
 */
void statement::bind(int index, const void *blob, std::int64_t size) {
   auto rc = sqlite3_bind_blob64(stmt_, index, blob, size, SQLITE_TRANSIENT);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{fmt::format("[{}] Failed to bind parameter: {} in {}", rc, index, name_)};
   }
}

void statement::bind(std::string_view param, const std::string &value) {
   fill_parameters_map();
   return bind(parameters_.value()[param.data()], value);
}

void statement::bind(std::string_view param, std::string_view value) {
   fill_parameters_map();
   return bind(parameters_.value()[param.data()], value);
}

void statement::bind(std::string_view param, std::int32_t value) {
   fill_parameters_map();
   return bind(parameters_.value()[param.data()], value);
}

void statement::bind(std::string_view param, std::int64_t value) {
   fill_parameters_map();
   return bind(parameters_.value()[param.data()], value);
}

void statement::bind(std::string_view param, double value) {
   fill_parameters_map();
   return bind(parameters_.value()[param.data()], value);
}

void statement::bind(std::string_view param, const void *blob, std::int64_t size) {
   fill_parameters_map();
   return bind(parameters_.value()[param.data()], blob, size);
}

/**
 * Get a 32-bit integer value of a given result column
 * @param index Column index
 * @return Value
 */
std::int32_t statement::get_int(int index) {
   return sqlite3_column_int(stmt_, index);
}

/**
 * Get a 64-bit integer value of a given result column
 * @param index Column index
 * @return Value
 */
std::int64_t statement::get_int64(int index) {
   return sqlite3_column_int64(stmt_, index);
}

/**
 * Downcast a 8-byte floating point value to a 4-byte float
 * @param index Column index
 * @return Value
 */
float statement::get_float(int index) {
   return static_cast<float>(sqlite3_column_double(stmt_, index));
}

/**
 * Return a 8-byte floating point value
 * @param index Column index
 * @return Value
 */
double statement::get_double(int index) {
   return sqlite3_column_double(stmt_, index);
}

std::string statement::get_text(int index) {
   using value_type = std::string::value_type;

   auto num_bytes = static_cast<std::size_t>(sqlite3_column_bytes(stmt_, index));
   auto num_chars = num_bytes / sizeof(value_type);
   if (num_chars * sizeof(value_type) < num_bytes) {
      ++num_chars;
   }

   std::string result;
   result.resize(num_chars);

   auto from = reinterpret_cast<const char *>(sqlite3_column_text(stmt_, index));
   std::copy(from, from + num_bytes, std::begin(result));

   return result;
}

bool statement::is_int(int index) {
   return sqlite3_column_type(stmt_, index) == SQLITE_INTEGER;
}

bool statement::is_float(int index) {
   return sqlite3_column_type(stmt_, index) == SQLITE_FLOAT;
}

bool statement::is_text(int index) {
   return sqlite3_column_type(stmt_, index) == SQLITE_TEXT;
}

bool statement::is_blob(int index) {
   return sqlite3_column_type(stmt_, index) == SQLITE_BLOB;
}

bool statement::is_null(int index) {
   return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

/**
 * Evaluate the statement
 * @return one of SQLite result codes
 */
int statement::evaluate(const std::initializer_list<int> &allowed_results) {
   int rc = sqlite3_step(stmt_);
   using namespace std;
   if (find(begin(allowed_results), end(allowed_results), rc) == end(allowed_results)) {
      throw std::runtime_error{fmt::format("[{}] Failed to evaluate statement: {}", rc, name_)};
   }
   return rc;
}

void statement::reset() {
   auto rc = sqlite3_reset(stmt_);
   if (rc != SQLITE_OK) {
      throw std::runtime_error{fmt::format("[{}] Failed to reset statement: {}", rc, name_)};
   }
}

/**
 * Perform zero or more SQL statements without any result data
 * @param db Database
 * @param sql SQL statement
 */
void statement::exec(database &db, const char *sql) {
   char *errorMessage{nullptr};
   auto rc = sqlite3_exec(db.db_, sql, nullptr, nullptr, &errorMessage);

   if (rc != SQLITE_OK) {
      auto message = fmt::format("[{}] Failed to execute SQL: {}", rc, errorMessage);
      sqlite3_free(errorMessage);
      throw std::runtime_error{message};
   }
}
