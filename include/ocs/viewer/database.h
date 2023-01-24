//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_VIEWER_DATABASE_H
#define OCR_SUITE_VIEWER_DATABASE_H

#include <ocs/viewer/options.h>
#include <ocs/database.h>

#include <string>
#include <vector>

namespace ocs::viewer {

class database {
public:
   struct search_entry {
      std::string video_file_path;
      std::vector<ocs::database::search_entry> entries;
   };

public:
   explicit database(options options);

public:
   void collect_files();
   void find_text(const std::string &text);

private:
   options options_;

   std::vector<std::string> video_files_;
   std::vector<std::string> database_files_;
};

} // namespace ocs::viewer

#endif // OCR_SUITE_VIEWER_DATABASE_H
