//
// Created by Dennis Sitelew on 18.12.22.
//

#pragma once

#include <ocs/recognition/options.h>
#include <ocs/recognition/value_queue.h>

#include <ocs/ffmpeg/decoder.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ocs::recognition {

//! Class for getting frames from a video file, using the ffmpeg library.
class video {
public:
   using queue_t = value_queue<ffmpeg::decoder::frame>;
   using queue_ptr_t = std::shared_ptr<queue_t>;

public:
   video(const options &opts, queue_ptr_t queue, std::int64_t starting_frame = 0);

public:
   void start();

   std::optional<std::int64_t> frame_count() const;

   std::chrono::seconds frame_number_to_seconds(std::int64_t num) const;
   std::chrono::milliseconds frame_number_to_milliseconds(std::int64_t num) const;

private:
   ffmpeg::decoder::action on_frame(const AVFrame &ffmpeg_frame, std::int64_t frame_number);

private:
   queue_ptr_t queue_;
   ffmpeg::decoder decoder_;
};

} // namespace ocs::recognition
