//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_VIEWER_SEARCH_H
#define OCR_SUITE_VIEWER_SEARCH_H

#include <ocs/database.h>
#include <ocs/viewer/options.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ocs::viewer {

class results;

class search {
public:
   using db_entries_t = std::vector<ocs::database::search_entry>;

   struct search_entry {
      std::string video_file_path;
      db_entries_t entries;
   };

   class thread {
   public:
      struct database_file {
         std::string database_path;
         std::string video_path;
         std::uint64_t size;
      };

   public:
      explicit thread(search &db);
      ~thread();

   public:
      void run(const std::string &text);
      void add_file(const database_file &file);

   private:
      void start();
      void stop();

   private:
      void thread_func();
      void work_once();

   private:
      search *db_;

      std::mutex m_{};
      std::string search_text_{};
      std::condition_variable cv_{};
      bool should_start_{false};
      bool should_stop_{false};
      std::vector<database_file> files_{};
      db_entries_t current_entries_;

      std::thread thread_;
   };

public:
   explicit search(results &res_storage, options options);
   ~search();

private:
   void decrement_remaining_size(const thread::database_file &file, const db_entries_t &entries);

public:
   void collect_files();
   void find_text(const std::string &text);

   bool is_finished() const { return remaining_size_ == 0; }
   double get_progress() const { return 100.0 - (100.0 / total_size_) * remaining_size_; }
   results &get_results() { return *results_; }

private:
   options options_;

   std::vector<std::string> video_files_;
   std::vector<std::string> database_files_;

   std::mutex m_{};
   std::uint64_t remaining_size_{};
   std::uint64_t total_size_{};

   std::vector<std::unique_ptr<thread>> threads_{};
   results *results_;

   std::chrono::steady_clock::time_point search_start_;
};

} // namespace ocs::viewer

#endif // OCR_SUITE_VIEWER_SEARCH_H
