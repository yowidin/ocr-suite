//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

const int ocs::common::database::CURRENT_DB_VERSION = 4;

void ocs::common::database::db_update(sqlite_burrito::versioned_database &db, int from, std::error_code &ec) {
   spdlog::trace("Updating database: from version {}", from);

   switch (from) {
      case 0:
         return update_v0(db, ec);

      case 1:
         return update_v1(db, ec);

      case 2:
         return update_v2(db, ec);

      case 3:
         return update_v3(db, ec);

      default:
         ec = std::make_error_code(std::errc::invalid_argument);
         return;
   }
}