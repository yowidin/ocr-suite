//
// Created by Dennis Sitelew on 21.12.22.
//

#ifndef OCS_IDL_INCLUDE
#error Internal use only
#endif

const int ocs::database::CURRENT_DB_VERSION = 1;

void ocs::database::db_update(db_t &db, int from) {
   switch (from) {
      case 0:
         return update_v0(db);

      default:
         throw std::runtime_error("Unknown database version: " + std::to_string(from));
   }
}