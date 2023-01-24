//
// Created by Dennis Sitelew on 19.12.22.
//

#include <ocs/recognition/value_queue.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace ocs::recognition;

//! Make sure we can produce more data than our buffers queue can hold.
//! And also make sure we can shutdown the queue and the application.
TEST_CASE("Buffer Queue - slow consumer, fast producer", "[value_queue]") {
   constexpr size_t num_buffers = 5;

   value_queue<int> queue(num_buffers);

   atomic<int> num_produced{0};
   atomic<int> num_consumed{0};

   auto producer = [&queue, &num_produced]() {
      // Produce more buffers than we have in the queue.
      while (true) {
         auto opt_value = queue.get_producer_value();
         if (!opt_value.has_value()) {
            // We finished all the work
            break;
         }
         this_thread::sleep_for(chrono::milliseconds(10));
         *opt_value.value() = num_produced;
         queue.add_consumer_value(opt_value.value());
         std::cout << "-> " << num_produced << std::endl;
         ++num_produced;

         if (num_produced == num_buffers * 2) {
            std::cout << "Shutting down producer" << std::endl;
            queue.shutdown();
         }
      }
   };

   auto consumer = [&queue, &num_consumed]() {
      while (true) {
         auto opt_value = queue.get_consumer_value();
         if (!opt_value.has_value()) {
            // Finished consuming all the buffers.
            std::cout << "Shutting down consumer" << std::endl;
            break;
         }

         this_thread::sleep_for(chrono::milliseconds(100));
         auto num = *opt_value.value();
         queue.add_producer_value(opt_value.value());
         ++num_consumed;
         std::cout << "<- " << num << std::endl;
      }
   };

   thread producer_thread(producer);
   vector<thread> consumer_threads;
   for (int i = 0; i < 3; ++i) {
      consumer_threads.emplace_back(consumer);
   }

   producer_thread.join();
   for (auto &thread : consumer_threads) {
      thread.join();
   }

   REQUIRE(num_produced == num_consumed);
   REQUIRE(num_produced == num_buffers * 2);
}

TEST_CASE("Buffer Queue - slow producer, fast consumer", "[value_queue]") {
   constexpr size_t num_buffers = 5;

   value_queue<int> queue(num_buffers);

   atomic<int> num_produced{0};
   atomic<int> num_consumed{0};

   auto producer = [&queue, &num_produced]() {
      // Produce more buffers than we have in the queue.
      while (true) {
         auto opt_value = queue.get_producer_value();
         if (!opt_value.has_value()) {
            // We finished all the work
            break;
         }
         this_thread::sleep_for(chrono::milliseconds(100));
         *opt_value.value() = num_produced;
         queue.add_consumer_value(opt_value.value());
         std::cout << "-> " << num_produced << std::endl;
         ++num_produced;

         if (num_produced == num_buffers * 2) {
            std::cout << "Shutting down producer" << std::endl;
            queue.shutdown();
         }
      }
   };

   auto consumer = [&queue, &num_consumed]() {
      while (true) {
         auto opt_value = queue.get_consumer_value();
         if (!opt_value.has_value()) {
            // Finished consuming all the buffers.
            std::cout << "Shutting down consumer" << std::endl;
            break;
         }

         auto num = *opt_value.value();
         queue.add_producer_value(opt_value.value());
         ++num_consumed;
         std::cout << "<- " << num << std::endl;
      }
   };

   thread producer_thread(producer);
   vector<thread> consumer_threads;
   for (int i = 0; i < 3; ++i) {
      consumer_threads.emplace_back(consumer);
   }

   producer_thread.join();
   for (auto &thread : consumer_threads) {
      thread.join();
   }

   REQUIRE(num_produced == num_consumed);
   REQUIRE(num_produced == num_buffers * 2);
}
