//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/viewer/results.h>
#include <ocs/viewer/search.h>

#include <spdlog/spdlog.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>

#include <algorithm>

using namespace ocs::viewer;

namespace fs = boost::filesystem;

////////////////////////////////////////////////////////////////////////////////
/// Class: search::thread
////////////////////////////////////////////////////////////////////////////////
search::thread::thread(search &db)
   : db_{&db}
   , thread_([this]() { thread_func(); }) {
   // Nothing to do here
}

search::thread::~thread() {
   stop();
   thread_.join();
}

void search::thread::run(const std::string &text) {
   {
      std::unique_lock lock{m_};
      search_text_ = text;
   }
   start();
}

void search::thread::start() {
   {
      std::unique_lock lock{m_};
      should_start_ = true;
   }
   cv_.notify_one();
}

void search::thread::stop() {
   {
      std::unique_lock lock{m_};
      should_stop_ = true;
   }
   cv_.notify_one();
}

void search::thread::thread_func() {
   spdlog::trace("Starting search thread...");
   while (true) {
      std::unique_lock lock{m_};
      cv_.wait(lock, [this] { return should_stop_ || should_start_; });
      if (should_stop_) {
         break;
      }

      if (should_start_) {
         should_start_ = false;
         lock.unlock();
         work_once();
      }
   }
   spdlog::trace("Exiting search thread...");
}

void search::thread::add_file(const database_file &file) {
   std::unique_lock lock{m_};
   files_.push_back(file);
}

void search::thread::work_once() {
   for (const auto &f : files_) {
      try {
         ocs::common::database db(f.database_path, true);
         db.find_text(search_text_, current_entries_);
      } catch (const std::exception &e) {
         spdlog::error("Error searching {}: {}", f.database_path, e.what());
      } catch (...) {
         spdlog::error("Error searching {}: {}", f.database_path);
      }
      db_->decrement_remaining_size(f, current_entries_);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Class: database
////////////////////////////////////////////////////////////////////////////////
search::search(results &res, options options)
   : options_(std::move(options))
   , results_{&res} {
   auto max_threads = std::thread::hardware_concurrency() - 1;
   for (std::size_t i = 0; i < max_threads; ++i) {
      threads_.emplace_back(new thread(*this));
   }
}

search::~search() {
   threads_.clear();
}

void search::collect_files() {
   spdlog::debug("Collecting files...");

   video_files_.clear();
   database_files_.clear();

   total_size_ = 0;

   std::vector<std::uint64_t> thread_loads{};
   thread_loads.resize(threads_.size(), 0);

   auto get_next_index = [&] {
      using namespace std;
      return distance(begin(thread_loads), min_element(begin(thread_loads), end(thread_loads)));
   };

   std::vector<thread::database_file> search_files;

   // Find all database and video files
   fs::directory_iterator end_iter;
   for (fs::directory_iterator dir_itr(options_.video_dir); dir_itr != end_iter; ++dir_itr) {
      if (fs::is_regular_file(dir_itr->status())) {
         const auto path = dir_itr->path();
         auto extension = path.extension();

         if (extension == options_.db_extension) {
            auto as_string = path.string();
            spdlog::trace("Found a database file: {}", as_string);
            database_files_.emplace_back(as_string);

            auto size = fs::file_size(path);
            auto video_path = path;
            video_path.replace_extension(options_.video_extension);

            search_files.push_back({as_string, video_path.string(), size});
            total_size_ += size;
         }

         if (extension == options_.video_extension) {
            auto as_string = path.string();
            spdlog::trace("Found a video file: {}", as_string);
            video_files_.emplace_back(std::move(as_string));
         }
      }
   }

   // Sort the search files by size in order to distribute the load more evenly
   std::sort(std::begin(search_files), std::end(search_files),
             [](const auto &lhs, const auto &rhs) { return lhs.size < rhs.size; });

   for (const auto &entry : search_files) {
      const auto idx = get_next_index();
      thread_loads[idx] += entry.size;
      threads_[idx]->add_file(entry);
   }

   spdlog::debug("{} video and {} database files found", video_files_.size(), database_files_.size());
}

void search::find_text(const std::string &text) {
   const int min_text_length = 3;
   if (text.length() < min_text_length) {
      return;
   }

   remaining_size_ = total_size_;
   results_->clear();

   search_start_ = std::chrono::steady_clock::now();

   for (auto &t : threads_) {
      t->run(text);
   }
}

void search::decrement_remaining_size(const thread::database_file &file, const db_entries_t &entries) {
   std::unique_lock lock{m_};

   if (!entries.empty()) {
      results_->store({file.video_path, entries});
   }

   remaining_size_ -= file.size;
   if (remaining_size_ == 0) {
      const auto duration = std::chrono::steady_clock::now() - search_start_;
      spdlog::debug("Done in {}ms", std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
   }
}
