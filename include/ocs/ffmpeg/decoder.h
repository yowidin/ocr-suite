//
// Created by Dennis Sitelew on 11.03.23.
//

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct AVFrame;
struct AVPacket;

namespace ocs::ffmpeg {

class decoder {
private:
   class ffmpeg_data;

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

   enum class action { decode_next, stop };

   using frame_cb_t = std::function<action(const AVFrame &frame, std::int64_t frame_number)>;

public:
   decoder(std::string video_path, frame_filter filter, frame_cb_t cb, std::int64_t starting_frame = 0);
   ~decoder();

public:
   decoder(const decoder &) = delete;
   decoder(decoder &&) = delete;
   decoder &operator=(const decoder &) = delete;
   decoder &operator=(decoder &&) = delete;

public:
   void run();

   std::chrono::milliseconds frame_number_to_milliseconds(std::int64_t frame_number) const;

   std::chrono::seconds frame_number_to_seconds(std::int64_t frame_number) const;

   //! @return Total number of frames in the video file. Will return std::nullopt in case if video is not finalized yet.
   std::optional<std::int64_t> frame_count() const { return frame_count_; }

   //! Convert a FFMPEG frame into our internal representation
   void to_frame(const AVFrame &src, std::int64_t frame_number, frame &dst);

private:
   std::int64_t frame_number_to_timestamp(std::int64_t frame_number) const;

   void hw_decoder_init();

   bool seek_to_frame(std::int64_t frame_number);
   void seek_to_closest_frame(std::int64_t min_frame, std::int64_t max_frame, std::int64_t last_working);

   //! @return true if the decoding can continue, false otherwise
   bool handle_decoded_frames(AVPacket *packet);

   void convert_to_rgb_and_copy_data(const AVFrame &src, std::vector<std::uint8_t> &target);

private:
   const std::string path_;
   const frame_filter filter_;
   const frame_cb_t cb_;
   const std::int64_t starting_frame_;

   //! FFMPEG-related fields
   std::unique_ptr<ffmpeg_data> ffmpeg_;

   std::optional<std::int64_t> frame_count_{std::nullopt};
};

} // namespace ocs::ffmpeg
