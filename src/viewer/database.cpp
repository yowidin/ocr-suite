//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/viewer/database.h>

#include <spdlog/spdlog.h>
#include <boost/filesystem.hpp>

using namespace ocs::viewer;

namespace fs = boost::filesystem;

database::database(options options)
   : options_(std::move(options)) {
   // Nothing to do here
}

void database::collect_files() {
   SPDLOG_DEBUG("Collecting files...");

   video_files_.clear();
   database_files_.clear();

   fs::directory_iterator end_iter;
   for (fs::directory_iterator dir_itr(options_.video_dir); dir_itr != end_iter; ++dir_itr) {
      if (fs::is_regular_file(dir_itr->status())) {
         const auto path = dir_itr->path();
         auto extension = path.extension();

         if (extension == options_.db_extension) {
            auto as_string = path.string();
            SPDLOG_DEBUG("Found database file: {}", as_string);
            database_files_.emplace_back(std::move(as_string));
         }

         if (extension == options_.video_extension) {
            auto as_string = path.string();
            SPDLOG_DEBUG("Found video file: {}", as_string);
            video_files_.emplace_back(std::move(as_string));
         }
      }
   }
}

void database::find_text(const std::string &text) {
   const int min_text_length = 3;
   if (text.length() < min_text_length) {
      return;
   }

   SPDLOG_DEBUG("Searching for text: {}", text);
   for (const auto &db_file : database_files_) {
      SPDLOG_DEBUG("Searching in database file: {}", db_file);
      ocs::database db(db_file, true);
      std::vector<ocs::database::search_entry> entries;
      db.find_text(text, entries);
      if (!entries.empty()) {
         SPDLOG_DEBUG("Found {} entries", entries.size());
//         search_entries_.emplace_back(search_entry{db_file, entries});
      }
   }
   // TODO
}
