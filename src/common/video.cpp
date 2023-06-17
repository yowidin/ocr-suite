//
// Created by Dennis Sitelew on 18.12.22.
//

#include <ocs/common/util.h>
#include <ocs/common/video.h>

#include <spdlog/spdlog.h>

using namespace ocs::common;

////////////////////////////////////////////////////////////////////////////////
/// Class: video
////////////////////////////////////////////////////////////////////////////////
video::video(const std::string &path,
             ffmpeg::decoder::frame_filter filter,
             queue_ptr_t queue,
             std::int64_t starting_frame)
   : queue_{std::move(queue)}
   , decoder_{path, filter, [this](const auto &frame, auto num) { return on_frame(frame, num); }, starting_frame} {
   // Nothing to do here
}

ocs::ffmpeg::decoder::action video::on_frame(const AVFrame &ffmpeg_frame, std::int64_t frame_number) {
   using decoder_t = ocs::ffmpeg::decoder;

   auto opt_frame = queue_->get_producer_value();
   if (!opt_frame.has_value()) {
      // The queue is closed, we are done
      return decoder_t::action::stop;
   }

   auto &frame = opt_frame.value();

   decoder_.to_frame(ffmpeg_frame, frame_number, *frame);

   queue_->add_consumer_value(frame);

   return decoder_t::action::decode_next;
}

void video::start() {
   decoder_.run();
   queue_->shutdown();
}

std::optional<std::int64_t> video::frame_count() const {
   return decoder_.frame_count();
}

std::chrono::seconds video::frame_number_to_seconds(std::int64_t num) const {
   return decoder_.frame_number_to_seconds(num);
}

std::chrono::milliseconds video::frame_number_to_milliseconds(std::int64_t num) const {
   return decoder_.frame_number_to_milliseconds(num);
}
