//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCS_VIEWER_IDL_INCLUDE
#error Internal use only
#endif

const int ocs::viewer::results::CURRENT_DB_VERSION = 1;

void ocs::viewer::results::db_update(sqlite_burrito::versioned_database &db, int from, std::error_code &ec) {
   spdlog::trace("Updating database: from version {}", from);

   switch (from) {
      case 0:
         return update_v0(db);

      default:
         ec = std::make_error_code(std::errc::invalid_argument);
         return;
   }
}