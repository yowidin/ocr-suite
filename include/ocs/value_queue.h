//
// Created by Dennis Sitelew on 19.12.22.
//

#ifndef OCR_SUITE_VALUE_QUEUE_H
#define OCR_SUITE_VALUE_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace ocs {

/**
 * Holds a limited list of value pointers, allowing a single producer to generate values and multiple consumers to
 * consume them.
 */
template <typename T>
class value_queue {
public:
   using value_t = T;
   using value_ptr_t = std::shared_ptr<T>;
   using value_list_t = std::deque<value_ptr_t>;
   using value_ptr_opt_t = std::optional<value_ptr_t>;

public:
   value_queue(std::size_t max_objects);

public:
   value_queue(const value_queue &) = delete;
   value_queue &operator=(const value_queue &) = delete;

public:
   //! Block until a producer value is available and return it.
   value_ptr_opt_t get_producer_value();

   //! Block until a consumer value is available and return it.
   value_ptr_opt_t get_consumer_value();

   //! Add a consumer value to the queue.
   void add_consumer_value(const value_ptr_t &value);

   //! Add a producer value to the queue.
   void add_producer_value(const value_ptr_t &value);

   //! Shutdown the queue and notify all waiting threads.
   void shutdown();

private:
   //! Mutex protecting the producer queue.
   std::mutex producer_mutex_{};
   std::condition_variable producer_cv_{};

   //! Used by the producer thread to signal that the work is done - consumers
   //! should stop finish all the remaining values and stop waiting.
   std::atomic<bool> stop_producer_{false};

   //! Set to true automatically when the producer thread is done and all the
   //! values are consumed.
   std::atomic<bool> stop_consumer_{false};

   //! Buffers, available for writing data into them (producer).
   value_list_t producer_values_{};

   //! Mutex protecting the consumer queue.
   std::mutex consumer_mutex_{};
   std::condition_variable consumer_cv_{};

   //! Buffers, available for reading data from them (consumers).
   value_list_t consumer_values_{};
};

////////////////////////////////////////////////////////////////////////////////
/// Class: value_queue
////////////////////////////////////////////////////////////////////////////////
template <typename T>
value_queue<T>::value_queue(size_t num_values) {
   for (size_t i = 0; i < num_values; ++i) {
      producer_values_.emplace_back(std::make_shared<value_t>());
   }
}

template <typename T>
typename value_queue<T>::value_ptr_opt_t value_queue<T>::get_producer_value() {
   std::unique_lock lock(producer_mutex_);
   producer_cv_.wait(lock, [this] { return !producer_values_.empty() || stop_producer_; });

   if (stop_producer_) {
      return std::nullopt;
   }

   auto value = producer_values_.front();
   producer_values_.pop_front();

   return value;
}

template <typename T>
typename value_queue<T>::value_ptr_opt_t value_queue<T>::get_consumer_value() {
   std::unique_lock lock(consumer_mutex_);
   consumer_cv_.wait(lock, [this] { return !consumer_values_.empty() || stop_consumer_; });

   if (stop_consumer_) {
      return std::nullopt;
   }

   auto value = consumer_values_.front();
   consumer_values_.pop_front();

   return value;
}

template <typename T>
void value_queue<T>::add_consumer_value(const value_ptr_t &value) {
   std::unique_lock lock(consumer_mutex_);
   consumer_values_.push_back(value);
   lock.unlock();
   consumer_cv_.notify_all();
}

template <typename T>
void value_queue<T>::add_producer_value(const value_ptr_t &value) {
   std::unique_lock lock{consumer_mutex_};
   producer_values_.push_back(value);

   if (consumer_values_.empty() && stop_producer_) {
      // There is no more work to do and all the values have been consumed.
      // Wake up all the consumers, so that we can gracefully shut down.
      stop_consumer_ = true;
   }

   lock.unlock();
   producer_cv_.notify_one();

   if (stop_consumer_) {
      consumer_cv_.notify_all();
   }
}

template <typename T>
void value_queue<T>::shutdown() {
   std::unique_lock producer_lock{producer_mutex_};
   stop_producer_ = true;
   producer_lock.unlock();
   producer_cv_.notify_one();

   std::unique_lock consumer_lock{consumer_mutex_};
   if (consumer_values_.empty()) {
      // There is no more work to do and all the values have been consumed.
      // Wake up all the consumers, so that we can gracefully shut down.
      stop_consumer_ = true;
   }
   consumer_lock.unlock();
   consumer_cv_.notify_all();
}

} // namespace ocs

#endif // OCR_SUITE_VALUE_QUEUE_H
