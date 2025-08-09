//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

const int database::CURRENT_DB_VERSION = 4;

inline void database::db_update(sqlite_burrito::versioned_database &con, int from, std::error_code &ec) {
   spdlog::trace("Updating database: from version {}", from);

   switch (from) {
      case 0:
         update_v0(con, ec);
         return;

      case 1:
         update_v1(con, ec);
         return;

      case 2:
         update_v2(con, ec);
         return;

      case 3:
         update_v3(con, ec);
         return;

      default:
         ec = std::make_error_code(std::errc::invalid_argument);
   }
}