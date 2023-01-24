//
// Created by Dennis Sitelew on 18.12.22.
//

#pragma once

#include <ocs/recognition/options.h>
#include <ocs/recognition/value_queue.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ocs::recognition {

/**
 * A class for getting a single frame from a video file, using the ffmpeg
 * library.
 */
class video {
private:
   class ffmpeg_video_stream;
   friend ffmpeg_video_stream;

public:
   enum class frame_filter : std::uint16_t {
      //! Only include the Intra-coded (I) frames.
      //! I-frames contain the entire image, and are used as keyframes.
      I = 0x01,

      //! Only include the Predicted (P) frames.
      //! P-frames contain the difference between the current frame and the
      //! previous frame.
      P = 0x02,

      //! Only include the Bi-directionally Predicted (B) frames.
      //! B-frames contain the difference between the current frame and the
      //! previous frame, and the difference between the current frame and the
      //! next frame.
      B = 0x04,

      //! Include I and P frames.
      I_and_P = I | P,

      //! Include I, P and B frames.
      all_frames = I | P | B,
   };

   struct frame {
      std::int64_t frame_number;
      std::vector<std::uint8_t> data;
      int width;
      int height;
      int bytes_per_line;
   };

   using queue_t = value_queue<frame>;
   using queue_ptr_t = std::shared_ptr<queue_t>;

public:
   video(const options &opts, queue_ptr_t queue, std::int64_t starting_frame = 0);
   ~video();

public:
   void start();

   [[nodiscard]] std::optional<std::int64_t> frame_count() const;
   [[nodiscard]] std::int64_t frame_number_to_seconds(std::int64_t num) const;

private:
   queue_ptr_t queue_;
   std::unique_ptr<ffmpeg_video_stream> stream_;
};

} // namespace ocs::recognition
