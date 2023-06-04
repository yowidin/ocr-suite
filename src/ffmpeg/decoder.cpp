//
// Created by Dennis Sitelew on 11.03.23.
//

#include <ocs/ffmpeg/decoder.h>
#include <ocs/ffmpeg/traits.h>

#include <spdlog/spdlog.h>

using namespace ocs::ffmpeg;

namespace {

enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
   const auto requested = reinterpret_cast<const AVPixelFormat *>(ctx->opaque);
   for (const AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
      if (*p == *requested) {
         return *p;
      }
   }

   spdlog::error("Failed to get HW surface format");
   return AV_PIX_FMT_NONE;
}

decoder::frame_filter picture_type_to_filter(AVPictureType type) {
   switch (type) {
      case AV_PICTURE_TYPE_I:
         return decoder::frame_filter::I;
      case AV_PICTURE_TYPE_P:
         return decoder::frame_filter::P;
      case AV_PICTURE_TYPE_B:
         return decoder::frame_filter::B;
      default:
         // Fallback to the I-frame filter for unknown types
         return decoder::frame_filter::I;
   }
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
/// Class: decoder::ffmpeg_data
////////////////////////////////////////////////////////////////////////////////
class decoder::ffmpeg_data {
public:
   traits::format_context input_ctx;
   traits::codec_context decoder_ctx;

   int video_stream_idx{-1};

   AVBufferRef *hw_device_ctx{nullptr};
   AVHWDeviceType hw_device_type{AV_HWDEVICE_TYPE_NONE};
   AVPixelFormat hw_pix_fmt{AV_PIX_FMT_NONE};

   SwsContext *sws_context{nullptr};

   double frame_ratio{0.0};
   double time_ratio{0.0};

   traits::frame rgb_frame;
};

////////////////////////////////////////////////////////////////////////////////
/// Class: decoder
////////////////////////////////////////////////////////////////////////////////
decoder::decoder(std::string video_path, frame_filter filter, frame_cb_t cb, std::int64_t starting_frame)
   : path_{std::move(video_path)}
   , filter_{filter}
   , cb_{std::move(cb)}
   , starting_frame_{starting_frame}
   , ffmpeg_{new ffmpeg_data} {
   static ffmpeg::traits::log_setup _{};

   auto &input_ctx = ffmpeg_->input_ctx;
   auto &decoder_ctx = ffmpeg_->decoder_ctx;

   /* open the input file */
   if (avformat_open_input(input_ctx, path_.c_str(), nullptr, nullptr) != 0) {
      throw std::runtime_error("Failed to open input file: " + path_);
   }

   if (avformat_find_stream_info(input_ctx, nullptr) < 0) {
      throw std::runtime_error("Failed to find stream information");
   }

   /* find the video stream information */
   const AVCodec *decoder{nullptr};
   const int stream_idx = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
   if (stream_idx < 0) {
      throw std::runtime_error("Failed to find a video stream in the input file");
   }
   ffmpeg_->video_stream_idx = stream_idx;

   const AVStream *video_stream = input_ctx->streams[stream_idx];
   if (avcodec_parameters_to_context(decoder_ctx, video_stream->codecpar) < 0) {
      throw std::runtime_error("Failed to copy codec parameters to decoder context");
   }

   // Set up either HW or SW decoding context
   try {
      ffmpeg_->hw_device_type = traits::get_default_hw_decoder_type();
      ffmpeg_->hw_pix_fmt = traits::find_pixel_format_for_decoder(decoder, ffmpeg_->hw_device_type);
      decoder_ctx->opaque = &ffmpeg_->hw_pix_fmt;
      decoder_ctx->get_format = get_hw_format;
      hw_decoder_init();
   } catch (const std::runtime_error &ex) {
      spdlog::error("Failed to initialize HW decoder: {}", ex.what());
      spdlog::info("Falling back to the software decoder");
      ffmpeg_->hw_pix_fmt = AV_PIX_FMT_NONE;
      decoder_ctx.reset();
      if (avcodec_parameters_to_context(decoder_ctx, video_stream->codecpar) < 0) {
         throw;
      }
   }

   if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) {
      throw std::runtime_error("Failed to open codec for stream #" + std::to_string(stream_idx));
   }

   // Seek to the starting frame
   ffmpeg_->frame_ratio = av_q2d(video_stream->avg_frame_rate);
   ffmpeg_->time_ratio = av_q2d(video_stream->time_base);

   seek_to_closest_frame(0, starting_frame, 0);
   auto frame_count = static_cast<std::int64_t>((static_cast<double>(ffmpeg_->input_ctx->duration) / AV_TIME_BASE) *
                                                ffmpeg_->frame_ratio);
   if (frame_count > 0) {
      frame_count_ = frame_count;
   }
}

decoder::~decoder() = default;

std::int64_t decoder::frame_number_to_timestamp(std::int64_t frame_number) const {
   const auto time_in_seconds = static_cast<double>(frame_number) / ffmpeg_->frame_ratio;
   return static_cast<std::int64_t>(time_in_seconds / ffmpeg_->time_ratio);
}

std::chrono::milliseconds decoder::frame_number_to_milliseconds(std::int64_t frame_number) const {
   const auto fractional_seconds = static_cast<double>(frame_number) / ffmpeg_->frame_ratio;
   const auto fractional_milliseconds = fractional_seconds * 1000.0;
   return std::chrono::milliseconds{static_cast<std::int64_t>(fractional_milliseconds)};
}

std::chrono::seconds decoder::frame_number_to_seconds(std::int64_t frame_number) const {
   return std::chrono::duration_cast<std::chrono::seconds>(frame_number_to_milliseconds(frame_number));
}

void decoder::hw_decoder_init() {
   if (av_hwdevice_ctx_create(&ffmpeg_->hw_device_ctx, ffmpeg_->hw_device_type, nullptr, nullptr, 0) < 0) {
      throw std::runtime_error("Failed to create specified HW device");
   }

   ffmpeg_->decoder_ctx->hw_device_ctx = av_buffer_ref(ffmpeg_->hw_device_ctx);
}

bool decoder::seek_to_frame(std::int64_t frame_number) {
   const auto starting_time = frame_number_to_timestamp(frame_number);
   int res = avformat_seek_file(ffmpeg_->input_ctx, ffmpeg_->video_stream_idx, 0, starting_time, starting_time,
                                AVSEEK_FLAG_FRAME);
   return res >= 0;
}

void decoder::seek_to_closest_frame(std::int64_t min_frame, std::int64_t max_frame, std::int64_t last_working) {
   if (max_frame == 0) {
      seek_to_frame(last_working);
      return;
   }

   if (seek_to_frame(max_frame)) {
      return;
   }

   auto middle_frame = min_frame + (max_frame - min_frame) / 2;
   if (middle_frame == min_frame || middle_frame == max_frame) {
      // We went too deep, no more divisions are possible
      seek_to_frame(last_working);
      return;
   }

   if (seek_to_frame(middle_frame)) {
      seek_to_closest_frame(middle_frame, max_frame, middle_frame);
   } else {
      seek_to_closest_frame(min_frame, middle_frame, last_working);
   }
}

void decoder::convert_to_rgb_and_copy_data(const AVFrame &src, std::vector<std::uint8_t> &target) {
   auto &rgb = *ffmpeg_->rgb_frame;
   rgb.format = AV_PIX_FMT_RGB24;
   rgb.width = src.width;
   rgb.height = src.height;

   const auto av_format = [](auto fmt) { return static_cast<AVPixelFormat>(fmt); };

   int ret = av_image_alloc(rgb.data, rgb.linesize, src.width, src.height, av_format(rgb.format), 1);
   if (ret < 0) {
      throw std::runtime_error("Could not allocate destination image");
   }

   auto sws_context = ffmpeg_->sws_context;

   // Convert the image from its native format to RGB
   sws_context = sws_getCachedContext(sws_context, src.width, src.height, av_format(src.format), rgb.width, rgb.height,
                                      av_format(rgb.format), 0, nullptr, nullptr, nullptr);
   sws_scale(sws_context, static_cast<const uint8_t *const *>(src.data), src.linesize, 0, src.height, rgb.data,
             rgb.linesize);

   auto size = av_image_get_buffer_size(av_format(rgb.format), rgb.width, rgb.height, 1);
   target.resize(size);

   ret =
       av_image_copy_to_buffer(target.data(), size, static_cast<const uint8_t *const *>(rgb.data),
                               static_cast<const int *>(rgb.linesize), av_format(rgb.format), rgb.width, rgb.height, 1);
   if (ret < 0) {
      throw std::runtime_error("Could not copy image to buffer");
   }

   av_freep(&rgb.data);
}

void decoder::to_frame(const AVFrame &src, std::int64_t frame_number, frame &target) {
   target.frame_number = frame_number;
   target.width = src.width;
   target.height = src.height;
   convert_to_rgb_and_copy_data(src, target.data);
   target.bytes_per_line = ffmpeg_->rgb_frame->linesize[0];
}

bool decoder::handle_decoded_frames(AVPacket *packet) {
   auto &decoder_ctx = ffmpeg_->decoder_ctx;

   int ret = avcodec_send_packet(decoder_ctx, packet);
   if (ret < 0) {
      spdlog::error("Error sending a packet for decoding: {}", ret);
      return false;
   }

   traits::frame rgb_frame;

   while (true) {
      traits::frame frame, sw_frame;

      ret = avcodec_receive_frame(decoder_ctx, frame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
         return true;
      } else if (ret < 0) {
         spdlog::error("Error during decoding: {}", ret);
         return false;
      }

      const auto frame_mask = static_cast<int>(picture_type_to_filter(frame->pict_type));
      const auto filter_mask = static_cast<int>(filter_);
      if ((frame_mask & filter_mask) == 0) {
         // Skip this frame
         continue;
      }

      AVFrame *tmp_frame;
      if (frame->format == ffmpeg_->hw_pix_fmt) {
         // retrieve data from GPU to CPU
         if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) {
            spdlog::error("Error transferring the data to system memory");
            return false;
         }
         tmp_frame = sw_frame.get();
      } else {
         tmp_frame = frame.get();
      }

      auto frame_number =
          static_cast<std::int64_t>(static_cast<double>(frame->pts) * ffmpeg_->time_ratio * ffmpeg_->frame_ratio);

      if (frame_number < starting_frame_) {
         // We weren't able to seek to the frame itself, so we have to skip some frames before it
         continue;
      }

      auto frame_action = cb_(*tmp_frame, frame_number);
      if (frame_action != action::decode_next) {
         // We are done here
         return false;
      }
   }
}

void decoder::run() {
   bool can_run = true;

   traits::packet packet;
   while (can_run) {
      int ret = av_read_frame(ffmpeg_->input_ctx, packet);
      if (ret < 0) {
         break;
      }

      if (ffmpeg_->video_stream_idx == packet->stream_index) {
         can_run = handle_decoded_frames(packet.get());
      }

      av_packet_unref(packet);
   }

   // flush the decoder
   if (can_run) {
      handle_decoded_frames(nullptr);
   }
}
